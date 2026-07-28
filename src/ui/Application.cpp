#include "Application.hpp"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <implot.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <iostream>

Application::Application(const std::string& windowTitle, int winWidth, int winHeight)
    : title(windowTitle), width(winWidth), height(winHeight), isRunning(false) {}

Application::~Application() {
    Close();
}

void Application::Run() {
    std::cout << "[DEBUG] Creating SDL window..." << std::endl;
    window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        std::cerr << "[SDL ERROR] Failed to create window: " << SDL_GetError() << std::endl;
        return;
    }

    std::cout << "[DEBUG] Creating SDL renderer..." << std::endl;
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "[SDL ERROR] Failed to create renderer: " << SDL_GetError() << std::endl;
        return;
    }

    std::cout << "[DEBUG] Initializing ImGui and ImPlot contexts..." << std::endl;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    float dpiScale = SDL_GetWindowDisplayScale(window);
    if (dpiScale <= 0.0f) dpiScale = 1.0f;

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(dpiScale);

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = dpiScale;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    std::cout << "[DEBUG] Linking SDL3-ImGui backend..." << std::endl;
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    isRunning = true;
    std::cout << "[DEBUG] Entering Main Loop." << std::endl;

    while (isRunning) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                isRunning = false;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) {
                isRunning = false;
            }
        }

        Update();

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        Render();

        ImGui::Render();
        
        SDL_SetRenderDrawColor(renderer, 26, 26, 26, 255);
        SDL_RenderClear(renderer);
        
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    std::cout << "[DEBUG] Exiting loop, destroying resources..." << std::endl;
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

void Application::Update() {
    if (window) {
        SDL_GetWindowSizeInPixels(window, &width, &height);
    }
}

void Application::Render() {
    ImGuiIO& io = ImGui::GetIO();
    float scale = io.FontGlobalScale;
    float navHeight = 45.0f * scale;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - navHeight));
    ImGuiWindowFlags workspaceFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("WorkspaceRegion", nullptr, workspaceFlags)) {
        if (activeTab == 0) {
            liveDashboardView.Render();
        } else if (activeTab == 1) {
            analysisWorkspaceView.Render(renderer); 
        }
        ImGui::End();
    }

    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - navHeight));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, navHeight));
    ImGuiWindowFlags navFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("BottomNav", nullptr, navFlags)) {
        float buttonWidth = 140.0f * scale;
        float buttonHeight = 30.0f * scale;
        float spacing = 10.0f * scale;
        float totalWidth = buttonWidth * 2 + spacing;
        
        ImGui::SetCursorPosX((io.DisplaySize.x - totalWidth) * 0.5f);
        ImGui::SetCursorPosY(7.0f * scale);

        if (ImGui::Button("Live Dashboard", ImVec2(buttonWidth, buttonHeight))) {
            activeTab = 0;
        }
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button("Analysis", ImVec2(buttonWidth, buttonHeight))) {
            activeTab = 1;
        }
        ImGui::End();
    }
}

void Application::Close() {
    isRunning = false;
}