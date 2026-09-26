#include "AnalysisWorkspaceView.hpp"
#include "../../io/SessionSerializer.hpp"
#include "../../utils/FileDialog.hpp"
#include "../../io/FileParser.hpp"
#include "../../analysis/SpatialResampler.hpp"
#include "../../analysis/LapSplitter.hpp"
#include "../components/TelemetryImportModal.hpp" 
#include <imgui.h>
#include <algorithm>
#include <fstream>
#include <iostream>

void AnalysisWorkspaceView::Render(SDL_Renderer* renderer) {
    float scale = ImGui::GetIO().FontGlobalScale;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Session")) {
                AnalysisTab newTab;
                newTab.title = "New Session";
                newTab.isOpen = true;
                newTab.needsFocus = true;
                newTab.sessionData = std::make_unique<Session>();
                openTabs.push_back(std::move(newTab));
            }
            
            if (ImGui::MenuItem("Open Session...")) {
                std::vector<SDL_DialogFileFilter> filters = { {"TensorStudio Session", "tss"}, {"All Files", "*"} };
                
                FileDialog::OpenFile(nullptr, [this](const std::string& filepath) {
                    if (!filepath.empty()) {
                        AnalysisTab loadedTab;
                        loadedTab.sessionData = std::make_unique<Session>();
                        
                        if (SessionSerializer::LoadSession(filepath, *loadedTab.sessionData)) {
                            loadedTab.sessionFilePath = filepath;
                            loadedTab.title = loadedTab.sessionData->sessionName;
                            loadedTab.isConfigured = !loadedTab.sessionData->drivers.empty(); 
                            loadedTab.isOpen = true;
                            loadedTab.needsFocus = true;
                            this->openTabs.push_back(std::move(loadedTab));
                        }
                    }
                }, filters);
            }

            ImGui::Separator();
            
            bool canSave = (activeTab != nullptr && activeTab->isConfigured);
            if (ImGui::MenuItem("Save", "Ctrl+S", false, canSave)) {
                if (activeTab->sessionFilePath.empty()) {
                    std::vector<SDL_DialogFileFilter> filters = { {"TensorStudio Session", "tss"} };
                    AnalysisTab* tabToSave = activeTab; 
                    
                    FileDialog::SaveFile(nullptr, "NewSession.tss", [tabToSave](const std::string& savePath) {
                        if (!savePath.empty()) {
                            std::string finalPath = savePath;
                            if (finalPath.find(".tss") == std::string::npos) finalPath += ".tss";
                            tabToSave->sessionFilePath = finalPath;
                            SessionSerializer::SaveSession(*tabToSave->sessionData, finalPath);
                        }
                    }, filters);
                } else {
                    SessionSerializer::SaveSession(*activeTab->sessionData, activeTab->sessionFilePath);
                }
            }
            
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, canSave)) {
                std::vector<SDL_DialogFileFilter> filters = { {"TensorStudio Session", "tss"} };
                AnalysisTab* tabToSave = activeTab;
                
                FileDialog::SaveFile(nullptr, "NewSession.tss", [tabToSave](const std::string& savePath) {
                    if (!savePath.empty()) {
                        std::string finalPath = savePath;
                        if (finalPath.find(".tss") == std::string::npos) finalPath += ".tss";
                        tabToSave->sessionFilePath = finalPath;
                        SessionSerializer::SaveSession(*tabToSave->sessionData, finalPath);
                    }
                }, filters);
            }
            
            ImGui::Separator();
            if (ImGui::MenuItem("Close")) { /* Logica per uscire */ }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Modify")) {
            bool canRecalculate = (activeTab != nullptr && activeTab->isConfigured);
            
            if (ImGui::MenuItem("Recalculate Sensor Fusion", nullptr, false, canRecalculate)) {
                for (const auto& driver : activeTab->sessionData->drivers) {
                    if (driver.telemetryPath.empty()) continue;
                    
                    if (GlobalTelemetryCache.find(driver.telemetryPath) != GlobalTelemetryCache.end()) {
                        auto& cache = GlobalTelemetryCache[driver.telemetryPath];
                        
                        // HAI RAGIONE TU: Peschiamo i dati RAW direttamente dalla memoria del progetto!
                        if (!cache.imuFrames.empty() && !cache.gnssFrames.empty()) {
                            SpatialResampler resampler;
                            auto fused = resampler.Resample(cache.imuFrames, cache.gnssFrames);
                            
                            LapSplitter splitter;
                            auto laps = splitter.SplitLaps(fused, activeTab->sessionData->selectedTrack, cache.baseTime);
                            
                            // Sovrascriviamo le curve vecchie con quelle nuove filtrate
                            cache.frames = fused;
                            cache.laps = laps;
                        } else {
                            std::cerr << "[WARNING] Impossibile ricalcolare: Dati RAW mancanti in memoria per " << driver.name << std::endl;
                        }
                    }
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | 
                                    ImGuiWindowFlags_NoResize | 
                                    ImGuiWindowFlags_NoMove;

    if (ImGui::BeginChild("Workspace Content", ImVec2(0, 0), false, window_flags)) {
        ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_Reorderable;

        if (ImGui::BeginTabBar("Sessions", tab_bar_flags)) {
            for (auto& tab : openTabs) {
                ImGuiTabItemFlags tabFlags = 0;

                if (tab.needsFocus) {
                    tabFlags |= ImGuiTabItemFlags_SetSelected;
                    tab.needsFocus = false; 
                }

                bool isSelected = ImGui::BeginTabItem(tab.title.c_str(), &tab.isOpen, tabFlags);
                if (isSelected) {
                    activeTab = &tab;
                    
                    if (!tab.isConfigured) {
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x / 4.0f));
                        
                        if (tab.setupView.Render(*tab.sessionData, renderer)) {
                            tab.isConfigured = true;
                            tab.title = std::string(tab.sessionData->sessionName); 
                        }
                    } 
                    else {
                        tab.worksheetView.Render(*tab.sessionData, tab.sessionData->splits, tab.activeSplitIndex, tab.openGraphLayouts);
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
                activeTab = nullptr;
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
    }
    ImGui::EndChild();
}