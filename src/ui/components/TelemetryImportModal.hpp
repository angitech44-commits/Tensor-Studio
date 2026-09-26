#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <ctime>
#include <unordered_map>
#include "../../analysis/LapSplitter.hpp"
#include "../../core/TelemetryData.hpp"
#include "../../core/Track.hpp"
#include "TrackMapView.hpp" 

// Global cache to prevent re-parsing data across the workspace
struct CachedTelemetry {
    std::vector<TelemetryFrame> frames;
    std::vector<ParsedLap> laps;
    double baseTime;
    
    // LISTE ORIGINALI PRE-FUSIONE
    std::vector<IMUFrame> imuFrames;
    std::vector<GNSSFrame> gnssFrames;
};
inline std::unordered_map<std::string, CachedTelemetry> GlobalTelemetryCache;

class TelemetryImportModal {
private:
    bool isOpen = false;
    bool shouldOpen = false;
    
    std::string* targetTelemetryPath = nullptr;
    std::string tempFilePath;
    Track activeTrack; 
    
    std::vector<ParsedLap> parsedLaps;
    std::vector<TelemetryFrame> fusedTelemetry;
    
    std::vector<IMUFrame> rawImu;
    std::vector<GNSSFrame> rawGnss;
    
    std::unique_ptr<std::mutex> dialogMutex;
    std::string pendingPath;
    bool triggerImport = false;

    int parsedRowCount = 0;
    double totalFileDuration = 0.0;
    double sessionBaseTime = 0.0;

    TrackMapView mapView; 
    bool needsMapCentering = false;

    void ParseTelemetryFile(const std::string& path);
    std::string FormatLapTime(float seconds);
    std::string FormatDate(double unixTime);
    std::string FormatDurationString(double totalSeconds);

public:
    TelemetryImportModal();
    void Open(std::string* telemetryPathPtr, const Track& track);
    void Render(SDL_Renderer* renderer); 
};