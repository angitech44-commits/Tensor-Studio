#pragma once
#include <string>
#include <vector>
#include "DriverEntry.hpp"
#include "Track.hpp"

// Spostiamo qui le enum e la struct che erano in SessionSetupView.hpp
enum class SplitType { FreePractice, Qualifying, Race };
enum class SplitDurationMode { Laps, Time };

struct SessionSplit {
    int startLap = 1;
    int endLap = 1;
    bool hasOutlap = true;
    bool hasInlap = true;
    SplitType type = SplitType::FreePractice;
    SplitDurationMode durationMode = SplitDurationMode::Laps;
    int durationMinutes = 15; 
    double absoluteStartTime = 0.0;
    double absoluteEndTime = 0.0;
};

struct Session {
    char sessionName[128];
    char sessionDate[64];
    Track selectedTrack;
    std::vector<DriverEntry> drivers;

    // NUOVO: La configurazione della timeline salvabile
    std::string referenceDriver = "";
    std::vector<SessionSplit> splits;

    Session();
};