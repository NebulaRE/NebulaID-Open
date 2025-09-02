#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#include <windows.h>
#include <iphlpapi.h>
#include <setupapi.h>
#include <winioctl.h>
#include <intrin.h>
#include <vector>
#include <optional>
#include <algorithm>
#include <cstdio>
#include <stdexcept>

#include "HwidGenerator.h"
#include "WmiHelper.h"
#include "SmbiosHelper.h"
#include "HashHelper.h"
#include "Util.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "setupapi.lib")

#ifndef GAA_FLAG_SKIP_ANYCAST
#define GAA_FLAG_SKIP_ANYCAST 0x0002
#endif
#ifndef GAA_FLAG_SKIP_MULTICAST
#define GAA_FLAG_SKIP_MULTICAST 0x0004
#endif
#ifndef GAA_FLAG_SKIP_DNS_SERVER
#define GAA_FLAG_SKIP_DNS_SERVER 0x0008
#endif

static std::optional<std::string> GetDiskSerialIoctl() {
    struct Candidate { std::string serial; bool removable = false; std::string vp; };
    std::vector<Candidate> cands;

    for (int i = 0; i < 8; ++i) {
        char path[64]; std::snprintf(path, sizeof(path), R"(\\.\PhysicalDrive%d)", i);
        HANDLE h = CreateFileA(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;

        STORAGE_PROPERTY_QUERY q{}; q.PropertyId = StorageDeviceProperty; q.QueryType = PropertyStandardQuery;
        STORAGE_DESCRIPTOR_HEADER hdr{}; DWORD br = 0;
        if (!DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), &hdr, sizeof(hdr), &br, nullptr)) { CloseHandle(h); continue; }

        if (hdr.Size < sizeof(STORAGE_DESCRIPTOR_HEADER) || hdr.Size > 16 * 1024) { CloseHandle(h); continue; }
        std::vector<BYTE> buf(hdr.Size);
        if (!DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), buf.data(), (DWORD)buf.size(), &br, nullptr)) { CloseHandle(h); continue; }

        auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buf.data());
        bool removable = desc->RemovableMedia != 0;

        std::string vendor, product, serial;
        if (desc->VendorIdOffset && desc->VendorIdOffset < buf.size())   vendor = util::Sanitize((char*)buf.data() + desc->VendorIdOffset);
        if (desc->ProductIdOffset && desc->ProductIdOffset < buf.size())  product = util::Sanitize((char*)buf.data() + desc->ProductIdOffset);
        if (desc->SerialNumberOffset && desc->SerialNumberOffset < buf.size()) serial = util::Sanitize((char*)buf.data() + desc->SerialNumberOffset);

        CloseHandle(h);

        std::string vp = util::ToUpper(vendor + " " + product);
        static const std::vector<std::string> bad = { "VIRTUAL", "VBOX", "VMWARE", "MSFT", "MICROSOFT", "QEMU", "XEN", "PARALLELS" };
        if (util::ContainsAny(vp, bad)) continue;

        if (!serial.empty() && !util::IsGenericJunk(serial)) cands.push_back({ serial, removable, vp });
    }

    auto it = std::find_if(cands.begin(), cands.end(), [](const Candidate& c) { return !c.removable; });
    if (it != cands.end()) return it->serial;
    if (!cands.empty()) return cands.front().serial;
    return std::nullopt;
}

static std::vector<std::string> GetPhysicalMacs() {
    std::vector<std::string> macs;
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG sz = 0;
    ULONG r = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, nullptr, &sz);
    if (r != ERROR_BUFFER_OVERFLOW || sz == 0) return macs;

    std::vector<uint8_t> buf(sz);
    auto* aa = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, aa, &sz) != NO_ERROR) return macs;

    struct Item { ULONG ifIndex; std::string mac; };
    std::vector<Item> items;

    for (auto* p = aa; p; p = p->Next) {
        if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK || p->IfType == IF_TYPE_TUNNEL || p->IfType == IF_TYPE_PPP) continue;
        if (p->PhysicalAddressLength != 6) continue;
        if (p->OperStatus != IfOperStatusUp) continue;

        uint8_t first = p->PhysicalAddress[0];
        if ((first & 0x02) != 0) continue; // locally administered

        char m[18];
        std::snprintf(m, sizeof(m), "%02X:%02X:%02X:%02X:%02X:%02X",
            p->PhysicalAddress[0], p->PhysicalAddress[1], p->PhysicalAddress[2],
            p->PhysicalAddress[3], p->PhysicalAddress[4], p->PhysicalAddress[5]);
        std::string mac = m;
        if (mac == "00:00:00:00:00:00" || mac == "02:00:00:00:00:00") continue;

        items.push_back({ p->IfIndex, mac });
    }

    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.ifIndex < b.ifIndex; });
    for (auto& it : items) macs.push_back(it.mac);
    return macs;
}

static std::string GetCpuSummary() {
    int r[4]{};
    __cpuid(r, 0);
    int maxId = r[0];

    char vendor[13] = { 0 };
    *reinterpret_cast<int*>(vendor + 0) = r[1]; // EBX
    *reinterpret_cast<int*>(vendor + 4) = r[3]; // EDX
    *reinterpret_cast<int*>(vendor + 8) = r[2]; // ECX

    __cpuid(r, 1);
    int eax1 = r[0], ecx1 = r[2], edx1 = r[3];

    int stepping = eax1 & 0xF;
    int model = (eax1 >> 4) & 0xF;
    int family = (eax1 >> 8) & 0xF;
    int extModel = (eax1 >> 16) & 0xF;
    int extFam = (eax1 >> 20) & 0xFF;
    if (family == 0xF) family += extFam;
    if (family == 0x6 || family == 0xF) model += (extModel << 4);

    int ebx7 = 0, ecx7 = 0, edx7 = 0;
    if (maxId >= 7) { int rr[4]{}; __cpuidex(rr, 7, 0); ebx7 = rr[1]; ecx7 = rr[2]; edx7 = rr[3]; }

    char brand[49] = { 0 };
    int ext[4]{};
    __cpuid(ext, 0x80000000);
    if (ext[0] >= 0x80000004) {
        int* b = reinterpret_cast<int*>(brand);
        __cpuid(b, 0x80000002);
        __cpuid(b + 4, 0x80000003);
        __cpuid(b + 8, 0x80000004);
    }

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "FAM:%d;MOD:%d;STP:%d;E1C:%08X;E1D:%08X;E7B:%08X;E7C:%08X;E7D:%08X;VEN:%s;BR:%s",
        family, model, stepping, ecx1, edx1, ebx7, ecx7, edx7, vendor, util::Sanitize(brand).c_str());

    return util::Sanitize(buf);
}

std::string HwidGenerator::Generate() {
    // 1) VM?
    if (WmiHelper::IsVirtualMachine()) return "virtual-detected";

    // 2) Coletas (com fallbacks simples e filtros de "junk")
    std::vector<std::pair<std::string, std::string>> parts;

    if (auto mb = WmiHelper::GetBaseboardSerial()) parts.emplace_back("MB", *mb);

    // Disco: IOCTL -> WMI (fallback)
    std::optional<std::string> disk = GetDiskSerialIoctl();
    if (!disk) disk = WmiHelper::GetDiskSerialWmi();
    if (disk && !util::IsGenericJunk(*disk)) parts.emplace_back("DSK", *disk);

    // CPU (CPUID)
    auto cpu = GetCpuSummary();
    if (!util::IsGenericJunk(cpu)) parts.emplace_back("CPU", cpu);

    // UUID do sistema
    if (auto uuid = WmiHelper::GetSystemUUID()) parts.emplace_back("UUID", *uuid);

    // BIOS serial
    if (auto bios = WmiHelper::GetBiosSerial()) parts.emplace_back("BIOS", *bios);

    // MACs físicos (ordenados)
    auto macs = GetPhysicalMacs();
    if (!macs.empty()) {
        std::string join;
        for (size_t i = 0; i < macs.size(); ++i) { if (i) join.push_back(';'); join += macs[i]; }
        parts.emplace_back("MACS", join);
    }

    std::vector<std::string> keys;
    for (auto& kv : parts) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end()); keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    if (keys.size() < 3) throw std::runtime_error("Insufficient sources to generate a stable HWID.");

    auto pick = [&](const char* k)->std::string {
        for (auto& kv : parts) if (kv.first == k) return kv.second; return {};
        };
    std::string concat;
    concat.reserve(1024);
    concat += "MB=" + pick("MB") + "|";
    concat += "DSK=" + pick("DSK") + "|";
    concat += "CPU=" + pick("CPU") + "|";
    concat += "UUID=" + pick("UUID") + "|";
    concat += "MACS=" + pick("MACS") + "|";
    concat += "BIOS=" + pick("BIOS");

    auto digest = hash256::SHA256(concat);
    auto hex = util::Hex(digest, /*upper*/true);
    return util::Grouped(hex, 4);
}
