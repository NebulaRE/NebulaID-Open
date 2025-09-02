#include "HwidGenerator.h"
#include "WmiHelper.h"
#include "Util.h"
#include <iostream>

int main() {
    try {
        auto mb = WmiHelper::GetBaseboardSerial().value_or("<none>");
        auto uuid = WmiHelper::GetSystemUUID().value_or("<none>");
        auto bios = WmiHelper::GetBiosSerial().value_or("<none>");

        std::cout << "Motherboard Serial : " << mb << "\n";
        std::cout << "System UUID        : " << uuid << "\n";
        std::cout << "BIOS Serial        : " << bios << "\n";

        auto hwid = HwidGenerator::Generate();
        std::cout << "HWID (SHA-256)     : " << hwid << "\n";
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
