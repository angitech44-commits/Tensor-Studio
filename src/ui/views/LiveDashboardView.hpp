#pragma once
#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include "../../analysis/LiveProcessor.hpp"
#include "../components/TrackMapView.hpp"
#include "../components/TrackManagerModal.hpp"
#include "../../core/Track.hpp"

enum class LiveState {
    Setup,
    Waiting,
    Active
};

class LiveDashboardView {
private:
    LiveProcessor processor;
    LiveState currentState = LiveState::Setup;
    
    Track selectedTrack;
    std::vector<std::string> availableTracks;
    char trackSearchBuffer[128] = "";
    
    TrackMapView mapView;
    TrackManagerModal trackManagerModal;
    
    bool showDebugView = false;
    
    // Indice per il motore di pre-caching offline delle Tile
    int cachePointIndex = -1;
    
    std::vector<double> imuTimes, accX, accY, accZ, gyroX, gyroY, gyroZ;
    std::vector<double> gnssTimes, speed, altitude;

    std::string FormatTime(float seconds);
    void DrawGBubble(ImVec2 center, float radius, float accX, float accY);
    void UpdateDebugPlotData();

public:
    LiveDashboardView();
    ~LiveDashboardView();

    void Render(SDL_Renderer* renderer);
};