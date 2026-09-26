#include "SessionSerializer.hpp"
#include "../core/Track.hpp"
#include "../core/TelemetryData.hpp"
#include "../analysis/LapSplitter.hpp" 
#include "../ui/views/SessionSetupView.hpp"
#include "../ui/components/TelemetryImportModal.hpp" 

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <SDL3/SDL.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

// MOTORE PER I PERCORSI SICURI (Android, iOS, Windows, Linux)
std::string SessionSerializer::GetSessionsDirectory() {
    char* prefPath = SDL_GetPrefPath("TensorStudio", "Sessions");
    std::string path = prefPath ? std::string(prefPath) : "./assets/sessions/";
    if (prefPath) SDL_free(prefPath);
    
    if (!fs::exists(path)) fs::create_directories(path);
    return path;
}

bool SessionSerializer::SaveSession(const Session& session, const std::string& filepath) {
    try {
        json j;
        j["sessionName"] = session.sessionName;

        j["referenceDriver"] = session.referenceDriver;
        json splitsArray = json::array();
        for (const auto& s : session.splits) {
            json splitObj;
            splitObj["startLap"] = s.startLap;
            splitObj["endLap"] = s.endLap;
            splitObj["hasOutlap"] = s.hasOutlap;
            splitObj["hasInlap"] = s.hasInlap;
            splitObj["type"] = static_cast<int>(s.type);
            splitObj["durationMode"] = static_cast<int>(s.durationMode);
            splitObj["durationMinutes"] = s.durationMinutes;
            splitObj["absoluteStartTime"] = s.absoluteStartTime;
            splitObj["absoluteEndTime"] = s.absoluteEndTime;
            splitsArray.push_back(splitObj);
        }
        j["splits"] = splitsArray;
        
        json trackJson;
        trackJson["name"] = session.selectedTrack.name;
        json fences = json::array();
        for (const auto& f : session.selectedTrack.fences) {
            json fenceObj;
            fenceObj["name"] = f.name;
            fenceObj["type"] = static_cast<int>(f.type);
            fenceObj["heading"] = f.directionHeading;
            
            json p1Array = json::array();
            p1Array.push_back(f.p1.lat);
            p1Array.push_back(f.p1.lon);
            fenceObj["p1"] = p1Array;
            
            json p2Array = json::array();
            p2Array.push_back(f.p2.lat);
            p2Array.push_back(f.p2.lon);
            fenceObj["p2"] = p2Array;
            
            fences.push_back(fenceObj);
        }
        trackJson["fences"] = fences;
        j["track"] = trackJson;

        json driversArray = json::array();
        for (const auto& driver : session.drivers) {
            json d;
            d["name"] = driver.name;
            d["telemetryPath"] = driver.telemetryPath; 

            if (!driver.telemetryPath.empty() && GlobalTelemetryCache.find(driver.telemetryPath) != GlobalTelemetryCache.end()) {
                
                const auto& cacheData = GlobalTelemetryCache.at(driver.telemetryPath);
                
                json telemetryJson;
                telemetryJson["baseTime"] = cacheData.baseTime;
                
                // LAPS
                json lapsJson = json::array();
                for (const auto& lap : cacheData.laps) { 
                    json lapObj;
                    lapObj["lapNumber"] = lap.lapNumber;
                    lapObj["timeSeconds"] = lap.timeSeconds;
                    lapObj["sectorTimes"] = lap.sectorTimes; 
                    lapObj["startTime"] = lap.startTime;
                    lapObj["endTime"] = lap.endTime;
                    lapObj["startLocalTime"] = lap.startLocalTime;
                    lapObj["endLocalTime"] = lap.endLocalTime;
                    lapsJson.push_back(lapObj);
                }
                telemetryJson["laps"] = lapsJson;

                // FUSED FRAMES
                json framesJson = json::array();
                for (const auto& f : cacheData.frames) {
                    json frameArray = json::array();
                    frameArray.push_back(f.time);
                    frameArray.push_back(f.lat);
                    frameArray.push_back(f.lon);
                    frameArray.push_back(f.speed);
                    frameArray.push_back(f.accX);
                    frameArray.push_back(f.accY);
                    frameArray.push_back(f.accZ);
                    frameArray.push_back(f.gyroX);
                    frameArray.push_back(f.gyroY);
                    frameArray.push_back(f.gyroZ);
                    framesJson.push_back(frameArray);
                }
                telemetryJson["frames"] = framesJson;

                // RAW IMU FRAMES
                json imuJson = json::array();
                for (const auto& i : cacheData.imuFrames) {
                    json arr = json::array();
                    arr.push_back(i.time);
                    arr.push_back(i.accX); arr.push_back(i.accY); arr.push_back(i.accZ);
                    arr.push_back(i.gyroX); arr.push_back(i.gyroY); arr.push_back(i.gyroZ);
                    arr.push_back(i.temp);
                    imuJson.push_back(arr);
                }
                telemetryJson["imuFrames"] = imuJson;

                // RAW GNSS FRAMES
                json gnssJson = json::array();
                for (const auto& g : cacheData.gnssFrames) {
                    json arr = json::array();
                    arr.push_back(g.time);
                    arr.push_back(g.lat); arr.push_back(g.lon); arr.push_back(g.alt);
                    arr.push_back(g.speed); arr.push_back(g.head);
                    arr.push_back(g.hacc); arr.push_back(g.sacc);
                    arr.push_back(g.sat); arr.push_back(g.fixType);
                    gnssJson.push_back(arr);
                }
                telemetryJson["gnssFrames"] = gnssJson;

                d["telemetryData"] = telemetryJson;
            }

            driversArray.push_back(d);
        }
        j["drivers"] = driversArray;

        // Assicura che la directory genitore esista (utile se l'utente salva fuori da GetSessionsDirectory)
        fs::path p(filepath);
        if (p.has_parent_path() && !fs::exists(p.parent_path())) {
            fs::create_directories(p.parent_path());
        }

        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;
        
        std::vector<uint8_t> v = json::to_msgpack(j);
        file.write(reinterpret_cast<const char*>(v.data()), v.size());
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SERIALIZER] Errore critico in salvataggio: " << e.what() << std::endl;
        return false;
    }
}

bool SessionSerializer::LoadSession(const std::string& filepath, Session& outSession) {
    try {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;

        std::vector<uint8_t> v((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        json j = json::from_msgpack(v);

        if (j.contains("sessionName")) {
            std::string sName = j["sessionName"].get<std::string>();
            strncpy(outSession.sessionName, sName.c_str(), sizeof(outSession.sessionName) - 1);
            outSession.sessionName[sizeof(outSession.sessionName) - 1] = '\0';
        }

        if (j.contains("referenceDriver")) {
            outSession.referenceDriver = j["referenceDriver"].get<std::string>();
        }

        outSession.splits.clear();
        if (j.contains("splits")) {
            for (const auto& s : j["splits"]) {
                SessionSplit split;
                split.startLap = s.value("startLap", 1);
                split.endLap = s.value("endLap", 1);
                split.hasOutlap = s.value("hasOutlap", true);
                split.hasInlap = s.value("hasInlap", true);
                split.type = static_cast<SplitType>(s.value("type", 0));
                split.durationMode = static_cast<SplitDurationMode>(s.value("durationMode", 0));
                split.durationMinutes = s.value("durationMinutes", 15);
                split.absoluteStartTime = s.value("absoluteStartTime", 0.0);
                split.absoluteEndTime = s.value("absoluteEndTime", 0.0);
                outSession.splits.push_back(split);
            }
        }

        if (j.contains("track")) {
            outSession.selectedTrack.name = j["track"]["name"].get<std::string>();
            outSession.selectedTrack.fences.clear();
            for (const auto& f : j["track"]["fences"]) {
                outSession.selectedTrack.fences.resize(outSession.selectedTrack.fences.size() + 1);
                auto& fence = outSession.selectedTrack.fences.back();
                
                fence.name = f.value("name", "");
                fence.type = static_cast<TrackFenceType>(f.value("type", 1));
                fence.directionHeading = f.value("heading", 0.0f);
                
                if (f.contains("p1") && f["p1"].size() >= 2) {
                    fence.p1.lat = f["p1"][0].get<double>();
                    fence.p1.lon = f["p1"][1].get<double>();
                }
                if (f.contains("p2") && f["p2"].size() >= 2) {
                    fence.p2.lat = f["p2"][0].get<double>();
                    fence.p2.lon = f["p2"][1].get<double>();
                }
            }
        }

        outSession.drivers.clear();
        if (j.contains("drivers")) {
            for (const auto& d : j["drivers"]) {
                DriverEntry driver;
                driver.name = d["name"].get<std::string>();
                driver.telemetryPath = d["telemetryPath"].get<std::string>();

                if (d.contains("telemetryData")) {
                    auto& tData = GlobalTelemetryCache[driver.telemetryPath];
                    tData.laps.clear();
                    tData.frames.clear();
                    tData.imuFrames.clear();
                    tData.gnssFrames.clear();

                    const auto& tdJson = d["telemetryData"];

                    tData.baseTime = tdJson.value("baseTime", 0.0);

                    // CARICAMENTO LAPS
                    for (const auto& l : tdJson["laps"]) {
                        tData.laps.resize(tData.laps.size() + 1);
                        auto& lap = tData.laps.back();
                        lap.lapNumber = l.value("lapNumber", 0);
                        lap.timeSeconds = l.value("timeSeconds", 0.0f);
                        if (l.contains("sectorTimes")) {
                            lap.sectorTimes = l["sectorTimes"].get<std::vector<float>>();
                        }
                        lap.startTime = l.value("startTime", 0.0);
                        lap.endTime = l.value("endTime", 0.0);
                        lap.startLocalTime = l.value("startLocalTime", "");
                        lap.endLocalTime = l.value("endLocalTime", "");
                    }

                    // CARICAMENTO FRAMES FUSI
                    if (tdJson.contains("frames")) {
                        for (const auto& f : tdJson["frames"]) {
                            tData.frames.resize(tData.frames.size() + 1);
                            auto& tf = tData.frames.back();
                            tf.time = f[0]; tf.lat = f[1]; tf.lon = f[2]; tf.speed = f[3];
                            tf.accX = f[4]; tf.accY = f[5]; tf.accZ = f[6];
                            tf.gyroX = f[7]; tf.gyroY = f[8]; tf.gyroZ = f[9];
                        }
                    }

                    // CARICAMENTO IMU RAW
                    if (tdJson.contains("imuFrames")) {
                        for (const auto& i : tdJson["imuFrames"]) {
                            tData.imuFrames.resize(tData.imuFrames.size() + 1);
                            auto& imf = tData.imuFrames.back();
                            imf.time = i[0];
                            imf.accX = i[1]; imf.accY = i[2]; imf.accZ = i[3];
                            imf.gyroX = i[4]; imf.gyroY = i[5]; imf.gyroZ = i[6];
                            imf.temp = i[7];
                        }
                    }

                    // CARICAMENTO GNSS RAW
                    if (tdJson.contains("gnssFrames")) {
                        for (const auto& g : tdJson["gnssFrames"]) {
                            tData.gnssFrames.resize(tData.gnssFrames.size() + 1);
                            auto& gf = tData.gnssFrames.back();
                            gf.time = g[0];
                            gf.lat = g[1]; gf.lon = g[2]; gf.alt = g[3];
                            gf.speed = g[4]; gf.head = g[5];
                            gf.hacc = g[6]; gf.sacc = g[7];
                            gf.sat = g[8]; gf.fixType = g[9];
                        }
                    }
                }
                
                outSession.drivers.push_back(driver);
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SERIALIZER] Errore critico in caricamento: " << e.what() << std::endl;
        return false;
    }
}