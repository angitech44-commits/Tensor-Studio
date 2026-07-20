#include "Application.hpp"
#include <raylib.h>
#include <rlImGui.h>
#include <imgui.h>

Application::Application(const std::string& windowTitle, int winWidth, int winHeight)
    : title(windowTitle), width(winWidth), height(winHeight), isRunning(false) {}

Application::~Application() {
    Close();
}

void Application::Run() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(width, height, title.c_str());
    SetTargetFPS(60);

    rlImGuiSetup(true);
    isRunning = true;

    while (!WindowShouldClose() && isRunning) {
        Update();
        BeginDrawing();
        ClearBackground(GetColor(0x1a1a1aff));

        rlImGuiBegin();
        Render();
        rlImGuiEnd();

        EndDrawing();
    }

    rlImGuiShutdown();
    CloseWindow();
}

void Application::Update() {
    width = GetScreenWidth();
    height = GetScreenHeight();
}

void Application::Render() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - 45.0f));
    ImGuiWindowFlags workspaceFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("WorkspaceRegion", nullptr, workspaceFlags)) {
        if (activeTab == 0) {
            liveDashboardView.Render();
        } else if (activeTab == 1) {
            analysisWorkspaceView.Render(currentSession, isProjectOpen, activeTab);
        }
        ImGui::End();
    }

    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - 45.0f));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 45.0f));
    ImGuiWindowFlags navFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("BottomNav", nullptr, navFlags)) {
        float buttonWidth = 140.0f;
        float totalWidth = buttonWidth * 2 + 10.0f;
        ImGui::SetCursorPosX((io.DisplaySize.x - totalWidth) * 0.5f);
        ImGui::SetCursorPosY(7.0f);

        if (ImGui::Button("Live Dashboard", ImVec2(buttonWidth, 30.0f))) {
            activeTab = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Analysis", ImVec2(buttonWidth, 30.0f))) {
            activeTab = 1;
        }
        ImGui::End();
    }
}

void Application::Close() {
    isRunning = false;
}