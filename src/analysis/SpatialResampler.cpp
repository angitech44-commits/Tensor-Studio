#include "SpatialResampler.hpp"
#include <algorithm>
#include <iostream>

float SpatialResampler::Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

double SpatialResampler::LerpDouble(double a, double b, float t) {
    return a + (b - a) * static_cast<double>(t);
}

void SpatialResampler::ApplyZeroPhaseEMA(std::vector<TelemetryFrame>& frames, float alphaX, float alphaY, float alphaZ) {
    if (frames.empty()) return;
    int n = frames.size();

    // 1. FORWARD PASS
    float sX = frames[0].accX, sY = frames[0].accY, sZ = frames[0].accZ;
    float sGx = frames[0].gyroX, sGy = frames[0].gyroY, sGz = frames[0].gyroZ;
    
    for (int i = 0; i < n; ++i) {
        sX = alphaX * frames[i].accX + (1.0f - alphaX) * sX;
        sY = alphaY * frames[i].accY + (1.0f - alphaY) * sY;
        sZ = alphaZ * frames[i].accZ + (1.0f - alphaZ) * sZ;
        
        // Per i giroscopi usiamo una via di mezzo fissa
        sGx = 0.1f * frames[i].gyroX + 0.9f * sGx;
        sGy = 0.1f * frames[i].gyroY + 0.9f * sGy;
        sGz = 0.1f * frames[i].gyroZ + 0.9f * sGz;
        
        frames[i].accX = sX; frames[i].accY = sY; frames[i].accZ = sZ;
        frames[i].gyroX = sGx; frames[i].gyroY = sGy; frames[i].gyroZ = sGz;
    }

    // 2. BACKWARD PASS (Annulla lo sfasamento temporale)
    sX = frames[n-1].accX; sY = frames[n-1].accY; sZ = frames[n-1].accZ;
    sGx = frames[n-1].gyroX; sGy = frames[n-1].gyroY; sGz = frames[n-1].gyroZ;
    
    for (int i = n - 1; i >= 0; --i) {
        sX = alphaX * frames[i].accX + (1.0f - alphaX) * sX;
        sY = alphaY * frames[i].accY + (1.0f - alphaY) * sY;
        sZ = alphaZ * frames[i].accZ + (1.0f - alphaZ) * sZ;
        
        sGx = 0.1f * frames[i].gyroX + 0.9f * sGx;
        sGy = 0.1f * frames[i].gyroY + 0.9f * sGy;
        sGz = 0.1f * frames[i].gyroZ + 0.9f * sGz;
        
        frames[i].accX = sX; frames[i].accY = sY; frames[i].accZ = sZ;
        frames[i].gyroX = sGx; frames[i].gyroY = sGy; frames[i].gyroZ = sGz;
    }
}

std::vector<TelemetryFrame> SpatialResampler::Resample(const std::vector<IMUFrame>& imuData, const std::vector<GNSSFrame>& gnssData) {
    std::vector<TelemetryFrame> fusedData;

    if (imuData.empty() || gnssData.empty()) {
        std::cerr << "[RESAMPLER] Errore: Uno dei sensori non ha dati." << std::endl;
        return fusedData;
    }

    double rawImuStart = imuData.front().time;

    std::vector<IMUFrame> validImu;
    validImu.reserve(imuData.size());
    for (const auto& frame : imuData) {
        if (frame.time > 1000000000000000.0) {
            validImu.push_back(frame);
        }
    }
    if (validImu.empty()) validImu = imuData;

    double commonStart = std::max(validImu.front().time, gnssData.front().time);
    double commonEnd = std::min(validImu.back().time, gnssData.back().time);
    
    if (commonEnd <= commonStart) return fusedData;

    float duration = static_cast<float>((commonEnd - commonStart) / 1000000.0);
    float dt = 0.01f;
    int estimatedSamples = static_cast<int>(duration / dt) + 1;
    fusedData.reserve(estimatedSamples);

    int imuIdx = 0;
    int gnssIdx = 0;
    int imuMax = validImu.size() - 1;
    int gnssMax = gnssData.size() - 1;

    for (float t = 0.0f; t <= duration; t += dt) {
        TelemetryFrame frame = {};
        
        double currentAbsoluteTime = commonStart + (static_cast<double>(t) * 1000000.0);
        frame.time = rawImuStart + (currentAbsoluteTime - validImu.front().time);

        // --- Allineamento IMU ---
        while (imuIdx < imuMax && validImu[imuIdx + 1].time < currentAbsoluteTime) {
            imuIdx++;
        }
        
        if (imuIdx < imuMax) {
            double t0 = validImu[imuIdx].time;
            double t1 = validImu[imuIdx + 1].time;
            float factor = (t1 > t0) ? static_cast<float>((currentAbsoluteTime - t0) / (t1 - t0)) : 0.0f;
            
            frame.accX = Lerp(validImu[imuIdx].accX, validImu[imuIdx + 1].accX, factor);
            frame.accY = Lerp(validImu[imuIdx].accY, validImu[imuIdx + 1].accY, factor);
            frame.accZ = Lerp(validImu[imuIdx].accZ, validImu[imuIdx + 1].accZ, factor);
            frame.gyroX = Lerp(validImu[imuIdx].gyroX, validImu[imuIdx + 1].gyroX, factor);
            frame.gyroY = Lerp(validImu[imuIdx].gyroY, validImu[imuIdx + 1].gyroY, factor);
            frame.gyroZ = Lerp(validImu[imuIdx].gyroZ, validImu[imuIdx + 1].gyroZ, factor);
            frame.temp = Lerp(validImu[imuIdx].temp, validImu[imuIdx + 1].temp, factor);
        } else {
            frame.accX = validImu[imuIdx].accX;
            frame.accY = validImu[imuIdx].accY;
            frame.accZ = validImu[imuIdx].accZ;
            frame.gyroX = validImu[imuIdx].gyroX;
            frame.gyroY = validImu[imuIdx].gyroY;
            frame.gyroZ = validImu[imuIdx].gyroZ;
            frame.temp = validImu[imuIdx].temp;
        }

        // --- Allineamento GNSS ---
        while (gnssIdx < gnssMax && gnssData[gnssIdx + 1].time < currentAbsoluteTime) {
            gnssIdx++;
        }

        if (gnssIdx < gnssMax) {
            double t0 = gnssData[gnssIdx].time;
            double t1 = gnssData[gnssIdx + 1].time;
            float factor = (t1 > t0) ? static_cast<float>((currentAbsoluteTime - t0) / (t1 - t0)) : 0.0f;
            
            frame.lat = LerpDouble(gnssData[gnssIdx].lat, gnssData[gnssIdx + 1].lat, factor) / 10000000.0;
            frame.lon = LerpDouble(gnssData[gnssIdx].lon, gnssData[gnssIdx + 1].lon, factor) / 10000000.0;
            
            frame.alt = Lerp(gnssData[gnssIdx].alt, gnssData[gnssIdx + 1].alt, factor);
            frame.speed = Lerp(gnssData[gnssIdx].speed, gnssData[gnssIdx + 1].speed, factor);
            frame.head = Lerp(gnssData[gnssIdx].head, gnssData[gnssIdx + 1].head, factor);
            
            frame.hacc = gnssData[gnssIdx].hacc;
            frame.sacc = gnssData[gnssIdx].sacc;
            frame.sat = gnssData[gnssIdx].sat;
            frame.fixType = gnssData[gnssIdx].fixType;
        } else {
            frame.lat = gnssData[gnssIdx].lat / 10000000.0;
            frame.lon = gnssData[gnssIdx].lon / 10000000.0;
            frame.alt = gnssData[gnssIdx].alt;
            frame.speed = gnssData[gnssIdx].speed;
            frame.head = gnssData[gnssIdx].head;
            frame.hacc = gnssData[gnssIdx].hacc;
            frame.sacc = gnssData[gnssIdx].sacc;
            frame.sat = gnssData[gnssIdx].sat;
            frame.fixType = gnssData[gnssIdx].fixType;
        }

        fusedData.push_back(frame);
    }

    // CORREZIONE ROTAZIONE MODULO (Così i grafici corrispondono al nome)
    for (auto& f : fusedData) {
        float oldX = f.accX;
        float oldY = f.accY;
        
        // Scambiamo gli assi. Se l'accelerazione risulta capovolta, cambia il segno (es. f.accX = -oldY)
        f.accX = oldY; 
        f.accY = -oldX; 
    }

    // APPLICAZIONE FILTRO ASIMMETRICO (Valori da 0.01 a 1.0. Più è basso, più taglia)
    // alphaX (ora è la vera Laterale): 0.15 = Filtro leggero per mantenere le curve perfette
    // alphaY (ora è la vera Longitudinale): 0.03 = Filtro pesante per disintegrare le vibrazioni del 390cc
    ApplyZeroPhaseEMA(fusedData, 0.07f, 0.03f, 0.10f);

    return fusedData;
}