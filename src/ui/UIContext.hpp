#pragma once
#include <raylib.h>
#include <imgui.h>
#include <implot.h>
#include "rlImGui.h"

namespace UIContext {
    inline void Init(int width, int height, const char* title) {
        SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(width, height, title);
        SetTargetFPS(60);

        rlImGuiSetup(true);
        ImPlot::CreateContext();
    }

    inline void BeginFrame() {
        rlImGuiBegin();
    }

    inline void EndFrame() {
        rlImGuiEnd();
    }

    inline void Shutdown() {
        ImPlot::DestroyContext();
        rlImGuiShutdown();
        CloseWindow();
    }
}