#include "AnalysisWorkspaceView.hpp"
#include <imgui.h>
#include <algorithm>

void AnalysisWorkspaceView::Render() {

    float menuBarHeight = 0.0f;


    if (ImGui::BeginMainMenuBar()) {
        
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Analysis")) { /* logica futura */ }
            if (ImGui::MenuItem("Open")) { /* logica futura */ }
            if (ImGui::MenuItem("Open Recent")) { /* logica futura */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Close")) { /*Close();*/ }
            if (ImGui::MenuItem("Save")) { /* logica futura */ }
            if (ImGui::MenuItem("Save As")) { /* logica futura */ }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("New Analysis")) { /* logica futura */ }
            if (ImGui::MenuItem("Open")) { /* logica futura */ }
            if (ImGui::MenuItem("Open Recent")) { /* logica futura */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Close")) { /*Close();*/ }
            if (ImGui::MenuItem("Save")) { /* logica futura */ }
            if (ImGui::MenuItem("Save As")) { /* logica futura */ }
            ImGui::EndMenu();
        }
        
        menuBarHeight = ImGui::GetWindowSize().y;
        
        ImGui::EndMainMenuBar();
    }
    
    ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight));
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - 45.0f - menuBarHeight));

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | 
                                ImGuiWindowFlags_NoResize | 
                                ImGuiWindowFlags_NoMove;

    ImGui::Begin("Workspace Content", nullptr, window_flags);

    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_Reorderable;

    

    if (ImGui::BeginTabBar("Sessions", tab_bar_flags)) {
    
        for (auto& tab : openTabs) {

            ImGuiTabItemFlags tabFlags = 0;

            if (tab.needsFocus) {
                tabFlags |= ImGuiTabItemFlags_SetSelected;
                tab.needsFocus = false; 
            }

            if (ImGui::BeginTabItem(tab.title.c_str(), &tab.isOpen, tabFlags)) {

                if (!tab.isConfigured) {

                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x / 4));
                    
                    if (tab.setupView.Render(*tab.sessionData)) {
                        
                        tab.isConfigured = true;
                        
                        tab.title = std::string(tab.sessionData->sessionName); 
                    }
                } 
                else {
                    
                    ImGui::Text("Dati caricati con successo per la sessione: %s", tab.sessionData->sessionName);
                }
                
                ImGui::EndTabItem();
            }
        }

        openTabs.erase(
            std::remove_if(openTabs.begin(), openTabs.end(), [](const AnalysisTab& t) { 
                return !t.isOpen; 
            }), 
            openTabs.end()
        );

        if (openTabs.empty()) {

            AnalysisTab startTab;
            startTab.title = "New Session";
            startTab.isOpen = true;
            startTab.isConfigured = false;

            startTab.sessionData = std::make_unique<Session>();
            
            openTabs.push_back(std::move(startTab));
        }

        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing)) {
            static int fallbackCounter = 1;

            AnalysisTab newTab;
            newTab.title = "New Session " + std::to_string(fallbackCounter);
            newTab.isOpen = true;
            newTab.needsFocus = true;

            newTab.sessionData = std::make_unique<Session>();
            
            openTabs.push_back(std::move(newTab));
            fallbackCounter++;
        }

        ImGui::EndTabBar();

    }

    ImGui::End();
}