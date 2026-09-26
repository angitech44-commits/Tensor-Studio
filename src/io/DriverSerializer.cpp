#include "DriverSerializer.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <SDL3/SDL.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

std::string DriverSerializer::GetDriversDirectory() {
    char* prefPath = SDL_GetPrefPath("TensorStudio", "Drivers");
    std::string path = prefPath ? std::string(prefPath) : "./assets/drivers/";
    if (prefPath) SDL_free(prefPath);
    
    if (!fs::exists(path)) fs::create_directories(path);
    return path;
}

std::vector<std::string> DriverSerializer::GetAvailableDrivers() {
    std::vector<std::string> drivers;
    std::string directoryPath = GetDriversDirectory();

    if (!fs::exists(directoryPath)) return drivers;

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
    if (!file.is_open()) return false;

    file << j.dump(4);
    return true;
}