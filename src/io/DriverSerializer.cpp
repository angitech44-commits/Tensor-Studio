#include "DriverSerializer.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>

using json = nlohmann::json;
namespace fs = std::filesystem;

#ifndef PROJECT_ROOT_DIR
#define PROJECT_ROOT_DIR "."
#endif

std::vector<std::string> DriverSerializer::GetAvailableDrivers() {
    std::vector<std::string> drivers;
    std::string directoryPath = std::string(PROJECT_ROOT_DIR) + "/assets/drivers";

    if (!fs::exists(directoryPath)) {
        fs::create_directories(directoryPath);
        return drivers;
    }

    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        if (entry.path().extension() == ".json") {
            drivers.push_back(entry.path().stem().string());
        }
    }
    return drivers;
}

bool DriverSerializer::LoadDriver(const std::string& filepath, DriverProfile& outDriver) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    json j;
    try {
        file >> j;
        if (j.contains("name")) outDriver.name = j["name"].get<std::string>();
    } catch (const std::exception& e) {
        std::cerr << "Errore parse JSON Driver: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool DriverSerializer::SaveDriver(const DriverProfile& driver, const std::string& filepath) {
    json j;
    j["name"] = driver.name;

    std::ofstream file(filepath);
    if (!file.is_open()) {
        fs::create_directories(fs::path(filepath).parent_path());
        file.open(filepath);
        if (!file.is_open()) return false;
    }

    file << j.dump(4);
    return true;
}