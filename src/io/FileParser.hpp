#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "../core/TelemetryData.hpp"

class FileParser {
public:
    std::vector<IMUFrame> imuData;
    std::vector<GNSSFrame> gnssData;
    std::vector<ErrorFrame> errorData;

    void ParseCSV(const std::string& filePath);
    void Clear();
    bool HasColumn(const std::string& name) const;
    int GetColumnIndex(const std::string& name) const;

private:
    std::unordered_map<std::string, int> colMap;
    double ParseDoubleFast(const char* start, const char* end) const;
};