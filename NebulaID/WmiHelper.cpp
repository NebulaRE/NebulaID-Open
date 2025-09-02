#define _WIN32_DCOM
#include "WmiHelper.h"
#include "SmbiosHelper.h"
#include "RegistryHelper.h"
#include "Util.h"
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#include <vector>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace {

    struct WmiSession {
        IWbemLocator* loc = nullptr;
        IWbemServices* svc = nullptr;
        WmiSession() {
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            HRESULT hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
                nullptr, EOAC_NONE, nullptr);
            if (hr == RPC_E_TOO_LATE) { /* ok */ }
            hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&loc);
            if (FAILED(hr)) return;
            BSTR ns = SysAllocString(L"ROOT\\CIMV2");
            hr = loc->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &svc);
            SysFreeString(ns);
            if (FAILED(hr)) { loc->Release(); loc = nullptr; return; }
            CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        }
        ~WmiSession() {
            if (svc) svc->Release();
            if (loc) loc->Release();
            CoUninitialize();
        }
        bool ok() const { return svc != nullptr; }
    };

} // anon

namespace WmiHelper {

    std::optional<std::wstring> QuerySingleString(const wchar_t* wql, const wchar_t* prop, long timeoutMs) {
        WmiSession s; if (!s.ok()) return std::nullopt;
        IEnumWbemClassObject* en = nullptr;
        BSTR qw = SysAllocString(L"WQL");
        BSTR q = SysAllocString(wql);
        HRESULT hr = s.svc->ExecQuery(qw, q, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
        SysFreeString(qw); SysFreeString(q);
        if (FAILED(hr) || !en) return std::nullopt;

        IWbemClassObject* obj = nullptr; ULONG ret = 0;
        std::optional<std::wstring> res;
        hr = en->Next(timeoutMs, 1, &obj, &ret);
        if (SUCCEEDED(hr) && ret == 1 && obj) {
            VARIANT vt; VariantInit(&vt);
            hr = obj->Get(prop, 0, &vt, nullptr, nullptr);
            if (SUCCEEDED(hr) && vt.vt == VT_BSTR && vt.bstrVal) res = std::wstring(vt.bstrVal, SysStringLen(vt.bstrVal));
            VariantClear(&vt); obj->Release();
        }
        en->Release();
        return res;
    }

    std::optional<std::string> GetBaseboardSerial() {
        auto s = QuerySingleString(L"SELECT SerialNumber FROM Win32_BaseBoard", L"SerialNumber");
        std::string a = util::Sanitize(util::W2U8(s.value_or(L"")));
        if (!util::IsGenericJunk(a)) return a;

        auto s2 = QuerySingleString(L"SELECT SerialNumber FROM Win32_SystemEnclosure", L"SerialNumber");
        a = util::Sanitize(util::W2U8(s2.value_or(L"")));
        if (!util::IsGenericJunk(a)) return a;

        auto info = ReadSmbiosInfo();
        if (info.baseboardSerial && !util::IsGenericJunk(*info.baseboardSerial)) return info.baseboardSerial;
        return std::nullopt;
    }

    std::optional<std::string> GetSystemUUID() {
        auto s = QuerySingleString(L"SELECT UUID FROM Win32_ComputerSystemProduct", L"UUID");
        std::string a = util::ToUpper(util::Sanitize(util::W2U8(s.value_or(L""))));
        if (!util::IsGenericJunk(a)) return a;

        auto info = ReadSmbiosInfo();
        if (info.systemUUID && !util::IsGenericJunk(*info.systemUUID)) return info.systemUUID;
        return std::nullopt;
    }

    std::optional<std::string> GetBiosSerial() {
        auto s = QuerySingleString(L"SELECT SerialNumber FROM Win32_BIOS", L"SerialNumber");
        std::string a = util::Sanitize(util::W2U8(s.value_or(L"")));
        if (!util::IsGenericJunk(a)) return a;

        auto info = ReadSmbiosInfo();
        if (info.biosSerial && !util::IsGenericJunk(*info.biosSerial)) return info.biosSerial;
        return std::nullopt;
    }

    std::optional<std::string> GetDiskSerialWmi() {
        // Retorna o primeiro SerialNumber válido de Win32_DiskDrive (se existir)
        WmiSession s; if (!s.ok()) return std::nullopt;
        IEnumWbemClassObject* en = nullptr;
        BSTR qw = SysAllocString(L"WQL");
        BSTR q = SysAllocString(L"SELECT SerialNumber FROM Win32_DiskDrive");
        HRESULT hr = s.svc->ExecQuery(qw, q, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
        SysFreeString(qw); SysFreeString(q);
        if (FAILED(hr) || !en) return std::nullopt;

        std::optional<std::string> out;
        while (true) {
            IWbemClassObject* obj = nullptr; ULONG ret = 0;
            hr = en->Next(2000, 1, &obj, &ret);
            if (FAILED(hr) || ret == 0) break;
            VARIANT vt; VariantInit(&vt);
            hr = obj->Get(L"SerialNumber", 0, &vt, nullptr, nullptr);
            if (SUCCEEDED(hr) && vt.vt == VT_BSTR && vt.bstrVal) {
                std::string s = util::Sanitize(util::W2U8(vt.bstrVal));
                if (!s.empty() && !util::IsGenericJunk(s)) { out = s; VariantClear(&vt); obj->Release(); break; }
            }
            VariantClear(&vt); obj->Release();
        }
        en->Release();
        return out;
    }

    bool IsVirtualMachine() {
        auto model = QuerySingleString(L"SELECT Model FROM Win32_ComputerSystem", L"Model");
        auto manuf = QuerySingleString(L"SELECT Manufacturer FROM Win32_ComputerSystem", L"Manufacturer");
        std::string m = util::ToUpper(util::W2U8(model.value_or(L"")));
        std::string mf = util::ToUpper(util::W2U8(manuf.value_or(L"")));
        static const std::vector<std::string> vmTokens = {
            "VIRTUAL", "VMWARE", "VBOX", "VIRTUALBOX", "KVM", "HYPER-V", "HYPERV", "QEMU", "XEN", "PARALLELS",
            "MICROSOFT CORPORATION VIRTUAL MACHINE"
        };
        if (util::ContainsAny(m, vmTokens) || util::ContainsAny(mf, vmTokens)) return true;

        // Registry fallback: BIOS version strings
        auto multi = reg::ReadHKLMMultiSZ(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemBiosVersion");
        for (auto& w : multi) {
            std::string s = util::ToUpper(util::W2U8(w));
            if (util::ContainsAny(s, vmTokens)) return true;
        }
        return false;
    }

}
