#include "Util.h"
#include <algorithm>
#include <cctype>
#include <windows.h>

namespace util {

    std::string ToUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::toupper(c); });
        return s;
    }

    void Trim(std::string& s) {
        auto issp = [](unsigned char c) { return std::isspace(c); };
        while (!s.empty() && issp(s.front())) s.erase(s.begin());
        while (!s.empty() && issp(s.back()))  s.pop_back();
    }

    std::string W2U8(const std::wstring& ws) {
        if (ws.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
        std::string out(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), out.data(), len, nullptr, nullptr);
        return out;
    }

    std::wstring U8ToW(const std::string& s) {
        if (s.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring out(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), len);
        return out;
    }

    bool IsAllZerosOrOnes(const std::string& s) {
        if (s.empty()) return true;
        bool allZ = std::all_of(s.begin(), s.end(), [](char c) { return c == '0'; });
        bool allO = std::all_of(s.begin(), s.end(), [](char c) { return c == '1'; });
        return allZ || allO;
    }

    bool ContainsAny(const std::string& hay, const std::vector<std::string>& needles) {
        for (const auto& n : needles) {
            if (hay.find(n) != std::string::npos) return true;
        }
        return false;
    }

    bool IsGenericJunk(const std::string& v) {
        if (v.empty()) return true;
        auto s = ToUpper(v);
        static const std::vector<std::string> bad = {
            "DEFAULT", "DEFAULT STRING", "DEFAULT-STRING", "N/A", "NA", "NONE", "UNKNOWN",
            "TO BE FILLED BY O.E.M.", "TO BE FILLED BY OEM", "SYSTEM PRODUCT NAME",
            "SYSTEM MANUFACTURER", "123456789", "00000000", "FFFFFFFF"
        };
        if (IsAllZerosOrOnes(s)) return true;
        for (auto& b : bad) if (s == b) return true;
        return false;
    }

    std::string Sanitize(const std::string& in) {
        std::string s = in;
        Trim(s);
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) {
            return c == '\r' || c == '\n' || c == '\t';
            }), s.end());
        return s;
    }

    std::string Hex(const std::vector<uint8_t>& bytes, bool upper) {
        static const char* hexd = "0123456789abcdef";
        static const char* hexu = "0123456789ABCDEF";
        const char* hx = upper ? hexu : hexd;
        std::string out; out.reserve(bytes.size() * 2);
        for (auto b : bytes) { out.push_back(hx[b >> 4]); out.push_back(hx[b & 0xF]); }
        return out;
    }

    std::string Grouped(const std::string& hex, size_t group) {
        if (group == 0) return hex;
        std::string out; out.reserve(hex.size() + hex.size() / group);
        for (size_t i = 0; i < hex.size(); ++i) {
            if (i > 0 && (i % group) == 0) out.push_back('-');
            out.push_back(hex[i]);
        }
        return out;
    }

}
