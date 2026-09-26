#include "FileParser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <algorithm>

void FileParser::Clear() {
    imuData.clear();
    gnssData.clear();
    errorData.clear();
    colMap.clear();
}

bool FileParser::HasColumn(const std::string& name) const {
    return colMap.find(name) != colMap.end();
}

int FileParser::GetColumnIndex(const std::string& name) const {
    auto it = colMap.find(name);
    return (it != colMap.end()) ? it->second : -1;
}

double FileParser::ParseDoubleFast(const char* start, const char* end) const {
    if (!start) return 0.0;
    double result = 0.0;
    double fraction = 0.0;
    double divisor = 1.0;
    bool negative = false;
    
    const char* p = start;
    while (*p == ' ' || *p == '\t') p++;
    
    if (*p == '-') { negative = true; p++; }
    else if (*p == '+') { p++; }
    
    while (*p >= '0' && *p <= '9') {
        result = result * 10.0 + (*p - '0');
        p++;
    }
    
    if (*p == '.' || *p == ',') {
        p++;
        while (*p >= '0' && *p <= '9') {
            fraction = fraction * 10.0 + (*p - '0');
            divisor *= 10.0;
            p++;
        }
    }
    
    result += fraction / divisor;
    return negative ? -result : result;
}

void FileParser::ParseCSV(const std::string& filePath) {
    Clear();
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[ERRORE] Impossibile aprire il file: " << filePath << std::endl;
        return;
    }

    std::string headerLine;
    bool headerFound = false;
    char delimiter = ',';

    while (std::getline(file, headerLine)) {
        std::string lowerLine = headerLine;
        for (char& c : lowerLine) {
            if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        }
        
        if (lowerLine.find("time") != std::string::npos) {
            headerFound = true;
            if (headerLine.find(';') != std::string::npos) delimiter = ';';
            break;
        }
    }

    if (!headerFound) {
        std::cerr << "[ERRORE FATALE] Nessuna riga contenente 'timestamp' o 'time' trovata." << std::endl;
        return;
    }

    std::stringstream ss(headerLine);
    std::string colName;
    int colIdx = 0;
    while (std::getline(ss, colName, delimiter)) {
        for (char& c : colName) {
            if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        }
        colName.erase(std::remove_if(colName.begin(), colName.end(), [](char c) {
            return c <= ' ';
        }), colName.end());
        
        if (colName == "timestamp" || colName.find("time") != std::string::npos) colMap["time"] = colIdx;
        else if (colName == "ax" || colName.find("accx") != std::string::npos) colMap["accx"] = colIdx;
        else if (colName == "ay" || colName.find("accy") != std::string::npos) colMap["accy"] = colIdx;
        else if (colName == "az" || colName.find("accz") != std::string::npos) colMap["accz"] = colIdx;
        else if (colName == "gx" || colName.find("gyrox") != std::string::npos) colMap["gyrox"] = colIdx;
        else if (colName == "gy" || colName.find("gyroy") != std::string::npos) colMap["gyroy"] = colIdx;
        else if (colName == "gz" || colName.find("gyroz") != std::string::npos) colMap["gyroz"] = colIdx;
        else if (colName == "temp" || colName.find("temperature") != std::string::npos) colMap["temp"] = colIdx;
        else if (colName == "lat" || colName.find("latitude") != std::string::npos) colMap["lat"] = colIdx;
        else if (colName == "lon" || colName.find("longitude") != std::string::npos) colMap["lon"] = colIdx;
        else if (colName == "alt" || colName.find("altitude") != std::string::npos) colMap["alt"] = colIdx;
        else if (colName == "vel" || colName.find("speed") != std::string::npos) colMap["speed"] = colIdx;
        else if (colName == "head" || colName.find("yaw") != std::string::npos) colMap["head"] = colIdx;
        else if (colName == "hacc") colMap["hacc"] = colIdx;
        else if (colName == "sacc") colMap["sacc"] = colIdx;
        else if (colName == "sat" || colName.find("satellites") != std::string::npos) colMap["sat"] = colIdx;
        else if (colName == "fixtype" || colName == "fix") colMap["fixtype"] = colIdx;
        else if (colName == "error" || colName == "err") colMap["error"] = colIdx;
        
        colIdx++;
    }

    int timeIdx  = GetColumnIndex("time");
    int accXIdx  = GetColumnIndex("accx");
    int accYIdx  = GetColumnIndex("accy");
    int accZIdx  = GetColumnIndex("accz");
    int gyroXIdx = GetColumnIndex("gyrox");
    int gyroYIdx = GetColumnIndex("gyroy");
    int gyroZIdx = GetColumnIndex("gyroz");
    int tempIdx  = GetColumnIndex("temp");

    int latIdx   = GetColumnIndex("lat");
    int lonIdx   = GetColumnIndex("lon");
    int altIdx   = GetColumnIndex("alt");
    int speedIdx = GetColumnIndex("speed");
    int headIdx  = GetColumnIndex("head");
    int haccIdx  = GetColumnIndex("hacc");
    int saccIdx  = GetColumnIndex("sacc");
    int satIdx   = GetColumnIndex("sat");
    int fixTypeIdx = GetColumnIndex("fixtype");
    int errorIdx = GetColumnIndex("error");

    if (timeIdx == -1) {
        std::cerr << "[ERRORE] Impossibile mappare colonna del tempo." << std::endl;
        return; 
    }

    std::string line;
    std::vector<const char*> tokens;
    tokens.reserve(64); 

    int debugImuCount = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        tokens.clear();
        const char* ptr = line.c_str();
        const char* end = ptr + line.length();
        
        while (ptr < end) {
            tokens.push_back(ptr);
            const char* sep = (const char*)memchr(ptr, delimiter, end - ptr);
            if (!sep) break;
            ptr = sep + 1;
        }

        if (tokens.size() <= timeIdx) continue;

        double currentTime = ParseDoubleFast(tokens[timeIdx], nullptr);

        if (accXIdx != -1 && tokens.size() > accXIdx && *tokens[accXIdx] != delimiter && *tokens[accXIdx] != '\0' && *tokens[accXIdx] != '\r') {
            IMUFrame imu = {}; 
            imu.time = currentTime;
            imu.accX  = static_cast<float>(ParseDoubleFast(tokens[accXIdx], nullptr));
            if (accYIdx != -1 && tokens.size() > accYIdx) imu.accY = static_cast<float>(ParseDoubleFast(tokens[accYIdx], nullptr));
            if (accZIdx != -1 && tokens.size() > accZIdx) imu.accZ = static_cast<float>(ParseDoubleFast(tokens[accZIdx], nullptr));
            if (gyroXIdx != -1 && tokens.size() > gyroXIdx) imu.gyroX = static_cast<float>(ParseDoubleFast(tokens[gyroXIdx], nullptr));
            if (gyroYIdx != -1 && tokens.size() > gyroYIdx) imu.gyroY = static_cast<float>(ParseDoubleFast(tokens[gyroYIdx], nullptr));
            if (gyroZIdx != -1 && tokens.size() > gyroZIdx) imu.gyroZ = static_cast<float>(ParseDoubleFast(tokens[gyroZIdx], nullptr));
            if (tempIdx != -1 && tokens.size() > tempIdx) imu.temp = static_cast<float>(ParseDoubleFast(tokens[tempIdx], nullptr));
            
            imuData.push_back(imu);

            // LOG DI DEBUG - Stampa i primi 15 frame letti
            if (debugImuCount < 15) {
                std::cout << "[DEBUG IMU " << debugImuCount + 1 << "] "
                          << "RAW_ax: '" << tokens[accXIdx] << "' -> " << imu.accX << " | "
                          << "RAW_ay: '" << (accYIdx != -1 && tokens.size() > accYIdx ? tokens[accYIdx] : "N/A") << "' -> " << imu.accY << " | "
                          << "RAW_az: '" << (accZIdx != -1 && tokens.size() > accZIdx ? tokens[accZIdx] : "N/A") << "' -> " << imu.accZ
                          << std::endl;
                debugImuCount++;
            }
        }

        if (latIdx != -1 && tokens.size() > latIdx && *tokens[latIdx] != delimiter && *tokens[latIdx] != '\0' && *tokens[latIdx] != '\r') {
            GNSSFrame gnss = {};
            gnss.time  = currentTime;
            gnss.lat   = ParseDoubleFast(tokens[latIdx], nullptr); 
            if (lonIdx != -1 && tokens.size() > lonIdx) gnss.lon = ParseDoubleFast(tokens[lonIdx], nullptr);
            if (altIdx != -1 && tokens.size() > altIdx) gnss.alt = static_cast<float>(ParseDoubleFast(tokens[altIdx], nullptr));
            if (speedIdx != -1 && tokens.size() > speedIdx) gnss.speed = static_cast<float>(ParseDoubleFast(tokens[speedIdx], nullptr));
            if (headIdx != -1 && tokens.size() > headIdx) gnss.head = static_cast<float>(ParseDoubleFast(tokens[headIdx], nullptr));
            if (haccIdx != -1 && tokens.size() > haccIdx) gnss.hacc = static_cast<float>(ParseDoubleFast(tokens[haccIdx], nullptr));
            if (saccIdx != -1 && tokens.size() > saccIdx) gnss.sacc = static_cast<float>(ParseDoubleFast(tokens[saccIdx], nullptr));
            if (satIdx != -1 && tokens.size() > satIdx) gnss.sat = static_cast<int>(ParseDoubleFast(tokens[satIdx], nullptr));
            if (fixTypeIdx != -1 && tokens.size() > fixTypeIdx) gnss.fixType = static_cast<int>(ParseDoubleFast(tokens[fixTypeIdx], nullptr));
            gnssData.push_back(gnss);
        }

        if (errorIdx != -1 && tokens.size() > errorIdx && *tokens[errorIdx] != delimiter && *tokens[errorIdx] != '\0' && *tokens[errorIdx] != '\r') {
            ErrorFrame err = {};
            err.time = currentTime;
            err.error = static_cast<int>(ParseDoubleFast(tokens[errorIdx], nullptr));
            errorData.push_back(err);
        }
    }
}