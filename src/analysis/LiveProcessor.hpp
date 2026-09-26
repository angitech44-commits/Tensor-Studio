#pragma once
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include "../core/TelemetryData.hpp"
#include "../core/Track.hpp"
#include "../utils/ThreadSafeQueue.hpp"
#include "../io/UDPReceiver.hpp"
#include "../io/FileLogger.hpp"

struct LiveLap {
    float lapTime = 0.0f;
    std::vector<GNSSFrame> trajectory;
};

class LiveProcessor {
private:
    ThreadSafeQueue<std::vector<uint8_t>> packetQueue;
    UDPReceiver receiver;

    std::atomic<bool> isProcessing{false};
    std::thread processorThread;
    mutable std::mutex dataMutex;
    
    FileLogger csvLogger; 
    Track activeTrack;
    
    IMUFrame latestImu = {};
    GNSSFrame latestGnss = {};
    TelemetryFrame latestFused = {}; // NUOVO: Dati allineati e fusi in RAM
    
    std::vector<IMUFrame> imuHistory;
    std::vector<GNSSFrame> gnssHistory;
    std::vector<TelemetryFrame> fusedHistory; // Storico dei dati processati pronti per UI/Analisi
    size_t maxHistorySize = 1000;
    
    // Motore Live Timing
    bool hasReceivedData = false;
    double currentLapStartTime = 0.0;
    float bestLapTime = 999.9f;
    float previousLapTime = 0.0f;
    
    std::vector<GNSSFrame> currentLapTrajectory;
    LiveLap bestLap;
    LiveLap previousLap;

    // --- MOTORE LIVE RESAMPLER ---
    float emaX = 0.0f, emaY = 0.0f, emaZ = 0.0f;
    float emaGx = 0.0f, emaGy = 0.0f, emaGz = 0.0f;
    bool emaInit = false;
    
    void ProcessLoop();
    void ParseLine(const std::string& line);
    void CheckFinishLineCrossing(const GNSSFrame& prev, const GNSSFrame& curr);
    
    float ExtractFloat(const std::string& line, const std::string& key);
    double ExtractDouble(const std::string& line, const std::string& key);

public:
    LiveProcessor();
    ~LiveProcessor();

    bool Start(const Track& track);
    void Stop();
    
    bool IsRunning() const { return isProcessing; }
    bool HasData() const { return hasReceivedData; }

    void GetLatestData(IMUFrame& outImu, GNSSFrame& outGnss) const;
    void GetLatestFused(TelemetryFrame& outFused) const; // Recupera il pacchetto unificato
    
    void GetTiming(float& current, float& previous, float& best) const;
    void GetHistory(std::vector<IMUFrame>& outImu, std::vector<GNSSFrame>& outGnss) const;
    void GetFusedHistory(std::vector<TelemetryFrame>& outFused) const;
    
    std::vector<GNSSFrame> GetCurrentTrajectory() const;
    std::vector<GNSSFrame> GetPreviousTrajectory() const;
    std::vector<GNSSFrame> GetBestTrajectory() const;
};