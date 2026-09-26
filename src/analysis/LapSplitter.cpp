#include "LapSplitter.hpp"
#include <cmath>
#include <algorithm>
#include <ctime>
#include <cstdio>

bool LapSplitter::SegmentsIntersect(double p0_x, double p0_y, double p1_x, double p1_y, 
                                    double p2_x, double p2_y, double p3_x, double p3_y, 
                                    double* t) {
    double s1_x = p1_x - p0_x;
    double s1_y = p1_y - p0_y;
    double s2_x = p3_x - p2_x;
    double s2_y = p3_y - p2_y;

    double denom = (-s2_x * s1_y + s1_x * s2_y);
    
    if (std::abs(denom) < 1e-15) return false; 

    double s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) / denom;
    double current_t = ( s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) / denom;

    if (s >= 0 && s <= 1 && current_t >= 0 && current_t <= 1) {
        if (t) *t = current_t;
        return true;
    }
    return false;
}

std::string LapSplitter::FormatLocalTime(double exactTimeSec, double sessionBaseTime, int tzOffsetSec) {
    if (exactTimeSec < 0.0) return "N/A";
    
    // Somma: Data dal file + Secondi del giro + Fuso Orario
    std::time_t t = static_cast<std::time_t>(sessionBaseTime + exactTimeSec) + tzOffsetSec;
    std::tm* tm_info = std::gmtime(&t);
    
    int ms = static_cast<int>((exactTimeSec - std::floor(exactTimeSec)) * 1000.0);
    char buf[32] = "N/A";
    if (tm_info) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, ms);
    }
    return std::string(buf);
}

std::vector<ParsedLap> LapSplitter::SplitLaps(const std::vector<TelemetryFrame>& telemetry, const Track& track, double sessionBaseTime) {
    std::vector<ParsedLap> laps;
    if (telemetry.size() < 2 || track.fences.empty()) return laps;

    TrackFence finishLine;
    std::vector<TrackFence> sectorLines;
    bool hasFinishLine = false;

    for (const auto& f : track.fences) {
        if (f.type == TrackFenceType::FinishLine) {
            finishLine = f;
            hasFinishLine = true;
        } else if (f.type == TrackFenceType::Sector) {
            sectorLines.push_back(f);
        }
    }

    if (!hasFinishLine) return laps;

    int trackTimezoneOffsetSec = 0;
    if (!track.fences.empty()) {
        trackTimezoneOffsetSec = static_cast<int>(std::round(track.fences[0].p1.lon / 15.0)) * 3600;
    }

    bool inLap = false;
    ParsedLap currentLap;
    double lastSplitTimeRaw = 0.0;
    double lastSplitTimeSec = 0.0;
    int lapCounter = 1;
    
    double cooldownEndTime = 0.0; 

    for (size_t i = 1; i < telemetry.size(); ++i) {
        const auto& p1 = telemetry[i - 1];
        const auto& p2 = telemetry[i];

        if (std::abs(p1.lat) < 1.0 || std::abs(p1.lon) < 1.0) continue;
        if (std::abs(p2.lat) < 1.0 || std::abs(p2.lon) < 1.0) continue;

        if (p2.time < cooldownEndTime) continue;

        double t_intersect = 0.0;
        
        if (SegmentsIntersect(p1.lon, p1.lat, p2.lon, p2.lat,
                              finishLine.p1.lon, finishLine.p1.lat,
                              finishLine.p2.lon, finishLine.p2.lat, &t_intersect)) {
            
            double exactTimeRaw = p1.time + t_intersect * (p2.time - p1.time);
            double exactTimeSec = exactTimeRaw / 1000000.0; 

            if (inLap) {
                currentLap.endTime = exactTimeSec;
                currentLap.endLocalTime = FormatLocalTime(exactTimeSec, sessionBaseTime, trackTimezoneOffsetSec);
                currentLap.timeSeconds = static_cast<float>(exactTimeSec - currentLap.startTime);
                currentLap.sectorTimes.push_back(static_cast<float>(exactTimeSec - lastSplitTimeSec));

                laps.push_back(currentLap);
                lapCounter++;
            }

            inLap = true;
            currentLap = ParsedLap();
            currentLap.lapNumber = lapCounter;
            currentLap.startTime = exactTimeSec;
            currentLap.startLocalTime = FormatLocalTime(exactTimeSec, sessionBaseTime, trackTimezoneOffsetSec);
            
            lastSplitTimeRaw = exactTimeRaw;
            lastSplitTimeSec = exactTimeSec;
            
            cooldownEndTime = exactTimeRaw + 2000000.0; 
            continue; 
        }

        if (inLap) {
            for (size_t s = 0; s < sectorLines.size(); ++s) {
                if (SegmentsIntersect(p1.lon, p1.lat, p2.lon, p2.lat,
                                      sectorLines[s].p1.lon, sectorLines[s].p1.lat,
                                      sectorLines[s].p2.lon, sectorLines[s].p2.lat, &t_intersect)) {
                    
                    double exactTimeRaw = p1.time + t_intersect * (p2.time - p1.time);
                    
                    if (exactTimeRaw - lastSplitTimeRaw > 3000000.0) {
                        double exactTimeSec = exactTimeRaw / 1000000.0;
                        currentLap.sectorTimes.push_back(static_cast<float>(exactTimeSec - lastSplitTimeSec));
                        
                        lastSplitTimeRaw = exactTimeRaw;
                        lastSplitTimeSec = exactTimeSec;
                        cooldownEndTime = exactTimeRaw + 2000000.0; 
                    }
                }
            }
        }
    }

    return laps;
}