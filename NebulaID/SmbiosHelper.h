#pragma once
#include <string>
#include <optional>

struct SmbiosInfo {
    std::optional<std::string> systemUUID;      // Type 1
    std::optional<std::string> biosSerial;      // Type 0
    std::optional<std::string> baseboardSerial; // Type 2
};

SmbiosInfo ReadSmbiosInfo();
