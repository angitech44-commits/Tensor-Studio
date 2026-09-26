#pragma once
#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include <memory>
#include "../../core/Session.hpp"
#include "SessionSetupView.hpp"
#include "DataWorksheetView.hpp"

struct AnalysisTab {
    std::string title;
    bool isOpen = true;
    bool isConfigured = false;
    bool needsFocus = false;
    std::unique_ptr<Session> sessionData;
    SessionSetupView setupView;
    DataWorksheetView worksheetView;
    int activeSplitIndex = 0;
    std::vector<std::string> openGraphLayouts;

    // NUOVO: Traccia dove è salvato il file per il salvataggio rapido
    std::string sessionFilePath = ""; 
};

class AnalysisWorkspaceView {
private:
    std::vector<AnalysisTab> openTabs;
    AnalysisTab* activeTab = nullptr; // Puntatore alla tab attualmente aperta a schermo

public:
    void Render(SDL_Renderer* renderer);
};