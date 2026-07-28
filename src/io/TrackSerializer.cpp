#include "TrackSerializer.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <cmath>

using json = nlohmann::json;
namespace fs = std::filesystem;

void to_json(json& j, const GeoPoint& p) {
    // 7 decimali per le coordinate GPS (precisione ~1.1 cm)
    auto roundGPS = [](double val) { return std::round(val * 10000000.0) / 10000000.0; };
    j = json{{"lat", roundGPS(p.lat)}, {"lon", roundGPS(p.lon)}};
}
void from_json(const json& j, GeoPoint& p) {
    j.at("lat").get_to(p.lat);
    j.at("lon").get_to(p.lon);
}

void to_json(json& j, const TrackFence& f) {
    // Arrotondiamo anche l'heading per non generare stringhe decimali inutilmente lunghe nel JSON
    auto roundHeading = [](double val) { return std::round(val * 10000000.0) / 10000000.0; };
    
    j = json{
        {"name", f.name}, 
        {"type", static_cast<int>(f.type)}, 
        {"p1", f.p1}, // Passa automaticamente dal to_json(GeoPoint) arrotondando
        {"p2", f.p2}, // Passa automaticamente dal to_json(GeoPoint) arrotondando
        {"heading", roundHeading(f.directionHeading)}
    };
}
void from_json(const json& j, TrackFence& f) {
    j.at("name").get_to(f.name);
    int type; j.at("type").get_to(type); f.type = static_cast<TrackFenceType>(type);
    j.at("p1").get_to(f.p1);
    j.at("p2").get_to(f.p2);
    j.at("heading").get_to(f.directionHeading);
}

bool TrackSerializer::SaveTrack(const Track& track, const std::string& filepath) {
    try {
        json j;
        j["name"] = track.name;
        
        // 3 decimali per la lunghezza (precisione al millimetro)
        auto roundLen = [](double val) { return std::round(val * 1000.0) / 1000.0; };
        j["length"] = roundLen(track.lengthMeters);
        
        j["fences"] = track.fences;
        j["limits"]["left"] = track.limits.leftBound;
        j["limits"]["right"] = track.limits.rightBound;

        std::ofstream file(filepath);
        if (!file.is_open()) return false;
        file << j.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[JSON Error] Failed to save track: " << e.what() << std::endl;
        return false;
    }
}

bool TrackSerializer::LoadTrack(const std::string& filepath, Track& outTrack) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) return false;
        
        json j;
        file >> j;
        
        j.at("name").get_to(outTrack.name);
        j.at("length").get_to(outTrack.lengthMeters);
        
        if (j.contains("fences")) j.at("fences").get_to(outTrack.fences);
        if (j.contains("limits")) {
            if (j["limits"].contains("left")) j["limits"]["left"].get_to(outTrack.limits.leftBound);
            if (j["limits"].contains("right")) j["limits"]["right"].get_to(outTrack.limits.rightBound);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[JSON Error] Failed to load track: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> TrackSerializer::GetAvailableTracks(const std::string& directory) {
    std::vector<std::string> tracks;
    
    std::string fullPath = std::string(PROJECT_ROOT_DIR) + "/" + directory;
    
    if (!std::filesystem::exists(fullPath)) {
        return tracks;
    }

    for (const auto& entry : std::filesystem::directory_iterator(fullPath)) {
        if (entry.path().extension() == ".json") {
            tracks.push_back(entry.path().stem().string());
        }
    }

    return tracks;
}