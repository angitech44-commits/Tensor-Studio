#include "LiveProcessor.hpp"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <SDL3/SDL.h>

LiveProcessor::LiveProcessor() : receiver(packetQueue) {
    imuHistory.reserve(maxHistorySize);
    gnssHistory.reserve(maxHistorySize);
    fusedHistory.reserve(maxHistorySize);
}

LiveProcessor::~LiveProcessor() {
    Stop();
}

bool LiveProcessor::Start(const Track& track) {
    if (isProcessing) return false;

    activeTrack = track;
    hasReceivedData = false;
    currentLapStartTime = 0.0;
    bestLapTime = 999.9f;
    previousLapTime = 0.0f;
    currentLapTrajectory.clear();
    bestLap.trajectory.clear();
    previousLap.trajectory.clear();
    
    imuHistory.clear();
    gnssHistory.clear();
    fusedHistory.clear();
    emaInit = false; // Reset dello stato del filtro EMA

    char* prefPath = SDL_GetPrefPath("TensorStudio", "Logs");
    std::string logDir = prefPath ? std::string(prefPath) : "logs/";
    if (prefPath) SDL_free(prefPath);

    if (!std::filesystem::exists(logDir)) {
        std::filesystem::create_directories(logDir);
    }

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    char buf[256];
    std::strftime(buf, sizeof(buf), "tensor_live_%Y%m%d_%H%M%S.csv", std::localtime(&in_time_t));
    
    std::string fullLogPath = logDir + buf;
    
    if (csvLogger.Open(fullLogPath)) {
        csvLogger.WriteLine("timestamp,ax,ay,az,gx,gy,gz,temp,lat,lon,vel,alt,head,hAcc,sAcc,sat,fixType,error");
    }

    if (!receiver.Start(8080)) {
        return false;
    }

    isProcessing = true;
    processorThread = std::thread(&LiveProcessor::ProcessLoop, this);
    return true;
}

void LiveProcessor::Stop() {
    if (!isProcessing) return;
    receiver.Stop();
    isProcessing = false;
    if (processorThread.joinable()) processorThread.join();
    csvLogger.Close();
}

double LiveProcessor::ExtractDouble(const std::string& line, const std::string& key) {
    size_t pos = line.find(key);
    if (pos != std::string::npos) {
        try { return std::stod(line.substr(pos + key.length())); } catch (...) {}
    }
    return 0.0;
}

float LiveProcessor::ExtractFloat(const std::string& line, const std::string& key) {
    size_t pos = line.find(key);
    if (pos != std::string::npos) {
        try { return std::stof(line.substr(pos + key.length())); } catch (...) {}
    }
    return 0.0f;
}

void LiveProcessor::CheckFinishLineCrossing(const GNSSFrame& prev, const GNSSFrame& curr) {
    if (activeTrack.fences.empty() || prev.lat == 0.0) return;
    
    for (const auto& fence : activeTrack.fences) {
        if (fence.type == TrackFenceType::FinishLine) {
            double dx1 = curr.lon - prev.lon;
            double dy1 = curr.lat - prev.lat;
            double dx2 = fence.p2.lon - fence.p1.lon;
            double dy2 = fence.p2.lat - fence.p1.lat;
            
            double det = dx1 * dy2 - dy1 * dx2;
            if (std::abs(det) > 1e-10) {
                double t1 = ((fence.p1.lon - prev.lon) * dy2 - (fence.p1.lat - prev.lat) * dx2) / det;
                double t2 = ((fence.p1.lon - prev.lon) * dy1 - (fence.p1.lat - prev.lat) * dx1) / det;
                
                if (t1 >= 0.0 && t1 <= 1.0 && t2 >= 0.0 && t2 <= 1.0) {
                    double crossTime = prev.time + t1 * (curr.time - prev.time);
                    double crossSec = crossTime / 1000000.0;
                    
                    if (currentLapStartTime > 0.0) {
                        float lapTime = static_cast<float>(crossSec - currentLapStartTime);
                        if (lapTime > 15.0f) { 
                            previousLapTime = lapTime;
                            previousLap.lapTime = lapTime;
                            previousLap.trajectory = currentLapTrajectory;
                            
                            if (lapTime < bestLapTime) {
                                bestLapTime = lapTime;
                                bestLap.lapTime = lapTime;
                                bestLap.trajectory = currentLapTrajectory;
                            }
                        }
                    }
                    currentLapStartTime = crossSec;
                    currentLapTrajectory.clear();
                }
            }
        }
    }
}

void LiveProcessor::ParseLine(const std::string& line) {
    if (line.find("IMU") != std::string::npos) {
        IMUFrame imu;
        imu.time = ExtractDouble(line, "time:");
        
        float rawX = ExtractFloat(line, "ax:");
        float rawY = ExtractFloat(line, "ay:");
        float rawZ = ExtractFloat(line, "az:");
        float rawGx = ExtractFloat(line, "gx:");
        float rawGy = ExtractFloat(line, "gy:");
        float rawGz = ExtractFloat(line, "gz:");
        imu.temp = ExtractFloat(line, "temp:");

        // 1. SCRITTURA CSV (Salviamo i dati grezzi esatti prima di inquinarli con i filtri)
        if (csvLogger.IsOpen()) {
            std::ostringstream oss;
            oss << (uint64_t)imu.time << ","
                << std::fixed << std::setprecision(4)
                << rawX << "," << rawY << "," << rawZ << ","
                << rawGx << "," << rawGy << "," << rawGz << "," 
                << std::setprecision(1) << imu.temp;
            csvLogger.WriteLine(oss.str());
        }

        // 2. LOGICA SPATIAL RESAMPLER LIVE
        float swappedX = rawY;
        float swappedY = -rawX;
        float swappedZ = rawZ;

        float alphaX = 0.07f;
        float alphaY = 0.03f;
        float alphaZ = 0.10f;

        if (!emaInit) {
            emaX = swappedX; emaY = swappedY; emaZ = swappedZ;
            emaGx = rawGx; emaGy = rawGy; emaGz = rawGz;
            emaInit = true;
        } else {
            emaX = alphaX * swappedX + (1.0f - alphaX) * emaX;
            emaY = alphaY * swappedY + (1.0f - alphaY) * emaY;
            emaZ = alphaZ * swappedZ + (1.0f - alphaZ) * emaZ;
            
            emaGx = 0.1f * rawGx + 0.9f * emaGx;
            emaGy = 0.1f * rawGy + 0.9f * emaGy;
            emaGz = 0.1f * rawGz + 0.9f * emaGz;
        }

        // 3. Compilazione pacchetto IMU pulito
        imu.accX = emaX; imu.accY = emaY; imu.accZ = emaZ;
        imu.gyroX = emaGx; imu.gyroY = emaGy; imu.gyroZ = emaGz;

        // 4. Fusione del TelemetryFrame
        TelemetryFrame fused = {};
        fused.time = imu.time;
        fused.accX = emaX; fused.accY = emaY; fused.accZ = emaZ;
        fused.gyroX = emaGx; fused.gyroY = emaGy; fused.gyroZ = emaGz;
        fused.temp = imu.temp;

        std::lock_guard<std::mutex> lock(dataMutex);
        
        // Zero-Order Hold per mantenere allineato il GNSS
        fused.lat = latestGnss.lat;
        fused.lon = latestGnss.lon;
        fused.speed = latestGnss.speed;
        fused.alt = latestGnss.alt;
        fused.head = latestGnss.head;
        fused.hacc = latestGnss.hacc;
        fused.sacc = latestGnss.sacc;
        fused.sat = latestGnss.sat;
        fused.fixType = latestGnss.fixType;

        latestImu = imu;
        latestFused = fused;
        
        imuHistory.push_back(imu);
        fusedHistory.push_back(fused);
        
        if (imuHistory.size() > maxHistorySize) imuHistory.erase(imuHistory.begin());
        if (fusedHistory.size() > maxHistorySize) fusedHistory.erase(fusedHistory.begin());
        
        hasReceivedData = true;
    } 
    else if (line.find("GNSS") != std::string::npos) {
        GNSSFrame gnss;
        double rawLat = ExtractDouble(line, "lat:");
        double rawLon = ExtractDouble(line, "lon:");
        float rawVel = ExtractFloat(line, "vel:");
        float rawAlt = ExtractFloat(line, "alt:");
        float rawHead = ExtractFloat(line, "head:");
        float rawHacc = ExtractFloat(line, "hAcc:");
        float rawSacc = ExtractFloat(line, "sAcc:");
        float rawSat = ExtractFloat(line, "sat:");
        float rawFix = ExtractFloat(line, "fix:");
        
        gnss.time = ExtractDouble(line, "time:");
        
        gnss.lat = rawLat / 10000000.0;
        gnss.lon = rawLon / 10000000.0;
        gnss.speed = rawVel; 
        gnss.alt = rawAlt;
        gnss.head = rawHead / 100000.0f;
        gnss.hacc = rawHacc;
        gnss.sacc = rawSacc;
        gnss.sat = static_cast<int>(rawSat);
        gnss.fixType = static_cast<int>(rawFix); 

        if (csvLogger.IsOpen()) {
            std::ostringstream oss;
            oss << (uint64_t)gnss.time << ",,,,,,,,"
                << (long long)rawLat << "," << (long long)rawLon << ","
                << (long long)rawVel << "," << (long long)rawAlt << ","
                << (long long)rawHead << "," << (long long)rawHacc << ","
                << (long long)rawSacc << "," << (int)rawSat << "," << (int)rawFix;
            csvLogger.WriteLine(oss.str());
        }

        std::lock_guard<std::mutex> lock(dataMutex);
        if (latestGnss.time > 0) {
            CheckFinishLineCrossing(latestGnss, gnss);
        }
        latestGnss = gnss;
        currentLapTrajectory.push_back(gnss);
        gnssHistory.push_back(gnss);
        if (gnssHistory.size() > maxHistorySize) gnssHistory.erase(gnssHistory.begin());
        hasReceivedData = true;
        
        if (currentLapStartTime == 0.0) currentLapStartTime = gnss.time / 1000000.0;
    }
}

void LiveProcessor::ProcessLoop() {
    std::vector<uint8_t> packet;
    while (isProcessing) {
        if (packetQueue.try_pop(packet)) {
            std::string line(packet.begin(), packet.end());
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
                line.pop_back();
            }
            if (!line.empty()) {
                ParseLine(line);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void LiveProcessor::GetLatestData(IMUFrame& outImu, GNSSFrame& outGnss) const {
    std::lock_guard<std::mutex> lock(dataMutex);
    outImu = latestImu;
    outGnss = latestGnss;
}

void LiveProcessor::GetLatestFused(TelemetryFrame& outFused) const {
    std::lock_guard<std::mutex> lock(dataMutex);
    outFused = latestFused;
}

void LiveProcessor::GetTiming(float& current, float& previous, float& best) const {
    std::lock_guard<std::mutex> lock(dataMutex);
    if (currentLapStartTime > 0.0) {
        current = static_cast<float>((latestGnss.time / 1000000.0) - currentLapStartTime);
    } else {
        current = 0.0f;
    }
    previous = previousLapTime;
    best = bestLapTime;
}

void LiveProcessor::GetHistory(std::vector<IMUFrame>& outImu, std::vector<GNSSFrame>& outGnss) const {
    std::lock_guard<std::mutex> lock(dataMutex);
    outImu = imuHistory;
    outGnss = gnssHistory;
}

void LiveProcessor::GetFusedHistory(std::vector<TelemetryFrame>& outFused) const {
    std::lock_guard<std::mutex> lock(dataMutex);
    outFused = fusedHistory;
}

std::vector<GNSSFrame> LiveProcessor::GetCurrentTrajectory() const { std::lock_guard<std::mutex> lock(dataMutex); return currentLapTrajectory; }
std::vector<GNSSFrame> LiveProcessor::GetPreviousTrajectory() const { std::lock_guard<std::mutex> lock(dataMutex); return previousLap.trajectory; }
std::vector<GNSSFrame> LiveProcessor::GetBestTrajectory() const { std::lock_guard<std::mutex> lock(dataMutex); return bestLap.trajectory; }