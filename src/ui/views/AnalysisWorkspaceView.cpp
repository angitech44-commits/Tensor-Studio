#include "AnalysisWorkspaceView.hpp"
#include <imgui.h>

void AnalysisWorkspaceView::Render(Session& session, bool& isProjectOpen, int& activeTab) {
    if (!isProjectOpen) {
        setupModal.Render(session, isProjectOpen, activeTab);
    } else {
        ImGui::Text("Telemetry Analysis Workspace (Project Loaded)");
    }
}