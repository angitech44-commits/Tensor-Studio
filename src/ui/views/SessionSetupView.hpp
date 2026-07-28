#pragma once
#include "../../core/Session.hpp"
#include "../components/TrackManagerModal.hpp"
#include "../components/TrackMapView.hpp" // IL NOSTRO MOTORE MAPPA
#include <SDL3/SDL.h>
#include <vector>
#include <string>

class SessionSetupView {
private:
    bool showValidationError = false;
    
    // Lista dinamica invece di quella hardcoded
    std::vector<std::string> availableTracks; 
    std::vector<std::string> availableDrivers = {"Luca", "Davide", "Alessandro"};

    TrackManagerModal trackManagerModal;
    
    // Nuovi strumenti di anteprima
    TrackMapView mapView;
    char trackSearchBuffer[128] = "";
    bool needsMapCentering = false;

public:
    bool Render(Session& session, SDL_Renderer* renderer);
};