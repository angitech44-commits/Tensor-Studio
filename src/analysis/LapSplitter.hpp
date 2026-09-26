#pragma once
#include <vector>
#include <string>
#include "../core/TelemetryData.hpp"
#include "../core/Track.hpp"

struct ParsedLap {
    int lapNumber = 0;
    float timeSeconds = 0.0f;
    std::vector<float> sectorTimes; 
    double startTime = 0.0;
    double endTime = 0.0;
    
    std::string startLocalTime; 
    std::string endLocalTime;   
};

class LapSplitter {
public:
    std::vector<ParsedLap> SplitLaps(const std::vector<TelemetryFrame>& telemetry, const Track& track, double sessionBaseTime = 0.0);

private:
    bool SegmentsIntersect(double p0_x, double p0_y, double p1_x, double p1_y, 
                           double p2_x, double p2_y, double p3_x, double p3_y, 
                           double* t_intersect);
                           
    std::string FormatLocalTime(double exactTimeSec, double sessionBaseTime, int tzOffsetSec);
};