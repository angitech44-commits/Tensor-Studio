#pragma once
#include "../../core/Session.hpp" // Adesso contiene gli split
#include "../components/TrackManagerModal.hpp"
#include "../components/TrackMapView.hpp" 
#include "../components/DriverManagerModal.hpp"
#include "../components/TelemetryImportModal.hpp"
#include "../../io/DriverSerializer.hpp"
#include "../../analysis/LapSplitter.hpp" 
#include <SDL3/SDL.h>
#include <vector>
#include <string>

class SessionSetupView {
private:
    bool showValidationError = false;
    
    std::vector<std::string> availableTracks; 
    std::vector<std::string> availableDrivers;

    // Le variabili "globalSplits" e "referenceDriver" non ci sono più qui,
    // leggeremo tutto direttamente da "session.splits" e "session.referenceDriver"
    
    std::string lastRefTelemetryPath = "";
    int cachedRefDriverLaps = 12;
    std::vector<ParsedLap> cachedRefLaps; 

    TrackManagerModal trackManagerModal;
    DriverManagerModal driverManagerModal;
    TelemetryImportModal telemetryImportModal;
    
    TrackMapView mapView;
    char trackSearchBuffer[128] = "";
    bool needsMapCentering = false;

    void UpdateRefTelemetry(const std::string& path, const Track& track);
    int CalculateEndLapByTime(int startLap, int durationMinutes);

public:
    bool Render(Session& session, SDL_Renderer* renderer);
    // Visto che gli splits ora sono in Session, puoi anche rimuovere GetSplits(),
    // ma la lasciamo per retrocompatibilità momentanea
    std::vector<SessionSplit> GetSplits(const Session& session) const { return session.splits; }
};