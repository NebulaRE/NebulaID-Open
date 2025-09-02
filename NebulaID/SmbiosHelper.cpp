#include "SmbiosHelper.h"
#include "Util.h"
#include <vector>
#include <windows.h>
#include <cstdint>

#pragma pack(push, 1)
struct RawHeader { uint8_t type; uint8_t length; uint16_t handle; };
#pragma pack(pop)

static std::vector<uint8_t> GetRawSMBIOS() {
    UINT sz = GetSystemFirmwareTable('RSMB', 0, nullptr, 0);
    if (!sz) return {};
    std::vector<uint8_t> buf(sz);
    if (GetSystemFirmwareTable('RSMB', 0, buf.data(), sz) != sz) return {};
    return buf;
}

static std::string DmiString(const uint8_t* start, size_t structLen, size_t index) {
    if (index == 0) return {};
    const char* p = reinterpret_cast<const char*>(start + structLen);
    if (*(const uint16_t*)p == 0) return {};
    size_t i = 1;
    while (*p) {
        const char* s = p;
        while (*p) ++p;
        if (i == index) return util::Sanitize(s);
        ++p; ++i;
    }
    return {};
}

static std::string UUIDHex(const uint8_t* uuid) {
    bool allSame = true; for (int i = 1; i < 16; ++i) if (uuid[i] != uuid[0]) { allSame = false; break; }
    if (allSame) return {};
    std::string out; out.reserve(32);
    static const char* hexd = "0123456789abcdef";
    for (int i = 0; i < 16; ++i) { unsigned b = uuid[i]; out.push_back(hexd[b >> 4]); out.push_back(hexd[b & 0xF]); }
    return util::ToUpper(out);
}

SmbiosInfo ReadSmbiosInfo() {
    SmbiosInfo info;
    auto raw = GetRawSMBIOS();
    if (raw.empty()) return info;

    const uint8_t* p = raw.data();
    const uint8_t* end = p + raw.size();
    while (p + sizeof(RawHeader) <= end) {
        const RawHeader* h = reinterpret_cast<const RawHeader*>(p);
        if (h->length == 0) break;
        const uint8_t* structEnd = p + h->length; if (structEnd > end) break;

        if (h->type == 0 && h->length >= 0x12) { // BIOS Info
            uint8_t idx = *(p + 0x07);
            auto s = DmiString(p, h->length, idx);
            if (!s.empty()) info.biosSerial = s;
        }
        else if (h->type == 1 && h->length >= 0x19) { // System Info
            const uint8_t* uuid = p + 0x08;
            auto hex = UUIDHex(uuid);
            if (!hex.empty()) info.systemUUID = hex;
        }
        else if (h->type == 2 && h->length >= 0x08) { // Baseboard
            uint8_t idx = *(p + 0x07);
            auto s = DmiString(p, h->length, idx);
            if (!s.empty()) info.baseboardSerial = s;
        }

        const uint8_t* str = structEnd;
        while (str + 1 < end && (str[0] != 0 || str[1] != 0)) ++str;
        if (str + 1 >= end) break;
        p = str + 2;
    }
    return info;
}
