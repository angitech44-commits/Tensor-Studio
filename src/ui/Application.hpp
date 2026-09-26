#pragma once
#include <string>
#include <SDL3/SDL.h> // Inclusione diretta per risolvere tutti i tipi di SDL nativamente
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

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_GLContext gl_context = nullptr;

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