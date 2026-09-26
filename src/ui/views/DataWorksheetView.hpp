#pragma once
#include <SDL3/SDL.h>
#include "../../core/Session.hpp"
#include "SessionSetupView.hpp"
#include "../layouts/LapComparisonLayout.hpp" // AGGIUNTO
#include <string>
#include <vector>
#include <unordered_map>

inline std::unordered_map<std::string, std::unordered_map<int, bool>> GlobalLapSelection;

struct DriverResult {
    std::string name;
    int lapsCompleted = 0;
    float bestLap = 0.0f;
    float totalTime = 0.0f;
    float bestS1 = 0.0f;
    float bestS2 = 0.0f;
    float bestS3 = 0.0f;
};

class DataWorksheetView {
private:
    LapComparisonLayout lapCompLayout; // AGGIUNTO

    std::string FormatTime(float seconds);
    std::string FormatGap(float seconds);
    std::string FormatGapStr(const DriverResult& current, const DriverResult& leader, bool isRace);

public:
    void Render(Session& session, const std::vector<SessionSplit>& splits, int& activeSplitIndex, std::vector<std::string>& openGraphLayouts);
};