#pragma once
#include <vector>
#include <cstddef>
#include <string>

namespace hash256 {

    // SHA-256 (BCrypt / CNG)
    std::vector<uint8_t> SHA256(const uint8_t* data, size_t len);

    // Conveniência
    inline std::vector<uint8_t> SHA256(const std::string& s) {
        return SHA256(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

}
