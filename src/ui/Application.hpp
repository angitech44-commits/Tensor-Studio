#pragma once
#include <string>
#include "views/LiveDashboardView.hpp"
#include "views/AnalysisWorkspaceView.hpp"
#include "../core/Session.hpp"

class Application {
private:
    std::string title;
    int width;
    int height;
    bool isRunning;
    int activeTab = -1;
    bool isProjectOpen = false;

    Session currentSession;
    LiveDashboardView liveDashboardView;
    AnalysisWorkspaceView analysisWorkspaceView;

    void Update();
    void Render();

public:
    Application(const std::string& windowTitle = "TensorStudio", int winWidth = 1280, int winHeight = 720);
    ~Application();

    void Run();
    void Close();
};