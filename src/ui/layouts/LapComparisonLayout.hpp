#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "../../core/Session.hpp"
#include "../components/TrackMapView.hpp"
#include <imgui.h>

class LapComparisonLayout {
private:
    TrackMapView miniMap;
    bool needsMapCentering = true;

    struct LapPlotData {
        std::string legendName;
        ImU32 color;
        std::vector<double> distance;
        std::vector<double> timeElapsed;
        std::vector<double> delta; 
        std::vector<double> speed;
        std::vector<double> latAcc;
        std::vector<double> lonAcc;
        std::vector<double> lats;
        std::vector<double> lons;
    };

    std::vector<LapPlotData> extractedData;
    std::string lastSelectionHash = "";

    bool isPlaying = false;
    // Modificati a double per poterli dare in pasto nativamente a ImPlot::DragLineX
    double currentPlaybackDist = 0.0;
    double maxPlaybackDist = 0.0;
    float playbackSpeedMultiplier = 1.0f;

    void ExtractSelectedLapsData(const Session& session);

public:
    LapComparisonLayout();
    void Render(const Session& session, SDL_Renderer* renderer);
};