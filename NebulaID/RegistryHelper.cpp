#include "RegistryHelper.h"
#include <windows.h>

namespace reg {

    std::optional<std::wstring> ReadHKLMString(const wchar_t* subKey, const wchar_t* valueName) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
            return std::nullopt;
        DWORD type = 0, size = 0;
        if (RegQueryValueExW(hKey, valueName, nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_SZ) {
            RegCloseKey(hKey); return std::nullopt;
        }
        std::wstring buf; buf.resize(size / sizeof(wchar_t));
        if (RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(&buf[0]), &size) == ERROR_SUCCESS && type == REG_SZ) {
            RegCloseKey(hKey);
            if (!buf.empty() && buf.back() == L'\0') buf.pop_back();
            return buf;
        }
        RegCloseKey(hKey);
        return std::nullopt;
    }

    std::vector<std::wstring> ReadHKLMMultiSZ(const wchar_t* subKey, const wchar_t* valueName) {
        std::vector<std::wstring> out;
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
            return out;
        DWORD type = 0, size = 0;
        if (RegQueryValueExW(hKey, valueName, nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_MULTI_SZ) {
            RegCloseKey(hKey); return out;
        }
        std::vector<wchar_t> buf(size / sizeof(wchar_t));
        if (RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(buf.data()), &size) == ERROR_SUCCESS && type == REG_MULTI_SZ) {
            const wchar_t* p = buf.data();
            while (*p) { size_t len = wcslen(p); out.emplace_back(p, p + len); p += len + 1; }
        }
        RegCloseKey(hKey);
        return out;
    }

}
