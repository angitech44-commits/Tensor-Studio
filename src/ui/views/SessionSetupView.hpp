#pragma once
#include "../../core/Session.hpp"
#include "../components/TrackManagerModal.hpp"
#include "../components/TrackMapView.hpp" 
#include "../components/DriverManagerModal.hpp"
#include "../../io/DriverSerializer.hpp"
#include <SDL3/SDL.h>
#include <vector>
#include <string>

enum class SplitType { FreePractice, Qualifying, Race };

struct SessionSplit {
    int startLap = 1;
    int endLap = 10;
    bool hasOutlap = true;
    bool hasInlap = true;
    SplitType type = SplitType::FreePractice;
};

class SessionSetupView {
private:
    bool showValidationError = false;
    
    std::vector<std::string> availableTracks; 
    std::vector<std::string> availableDrivers;

    // Timeline globale della sessione
    std::vector<SessionSplit> globalSplits;
    std::string referenceDriver = "";

    TrackManagerModal trackManagerModal;
    DriverManagerModal driverManagerModal;
    TrackMapView mapView;
    char trackSearchBuffer[128] = "";
    bool needsMapCentering = false;

public:
    bool Render(Session& session, SDL_Renderer* renderer);
};