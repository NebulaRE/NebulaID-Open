#include "HashHelper.h"
#include <windows.h>
#include <bcrypt.h>
#include <stdexcept>
#pragma comment(lib, "bcrypt.lib")

namespace hash256 {

    std::vector<uint8_t> SHA256(const uint8_t* data, size_t len) {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        if (st != 0) throw std::runtime_error("BCryptOpenAlgorithmProvider failed");

        DWORD objLen = 0, cb = 0, hashLen = 0;
        BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cb, 0);
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cb, 0);

        std::vector<uint8_t> obj(objLen), out(hashLen);
        BCRYPT_HASH_HANDLE hHash = nullptr;

        st = BCryptCreateHash(hAlg, &hHash, obj.data(), objLen, nullptr, 0, 0);
        if (st != 0) { BCryptCloseAlgorithmProvider(hAlg, 0); throw std::runtime_error("BCryptCreateHash failed"); }

        BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0);
        BCryptFinishHash(hHash, out.data(), (ULONG)out.size(), 0);

        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return out;
    }

}
