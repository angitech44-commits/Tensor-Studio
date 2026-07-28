#pragma once
#include "SessionSetupView.hpp"
#include "../../core/Session.hpp"
#include <vector>
#include <string>
#include <memory>

class AnalysisWorkspaceView {
private:

public:
    void Render(SDL_Renderer* renderer);
};

struct AnalysisTab {
    std::string title;
    bool isOpen = true;
    bool isConfigured = false;
    bool needsFocus = false;
    std::unique_ptr<Session> sessionData;
    SessionSetupView setupView;
};

inline std::vector<AnalysisTab> openTabs;