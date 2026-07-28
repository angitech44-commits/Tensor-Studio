#pragma once
#include "../core/DriverProfile.hpp"
#include <string>
#include <vector>

class DriverSerializer {
public:
    static std::vector<std::string> GetAvailableDrivers();
    static bool LoadDriver(const std::string& filepath, DriverProfile& outDriver);
    static bool SaveDriver(const DriverProfile& driver, const std::string& filepath);
};