#include "SessionSetupView.hpp"
#include <imgui.h>
#include <cstring>
#include <algorithm>
#include "../../io/TrackSerializer.hpp"
#include "../../utils/GeoMath.hpp"

#ifndef PROJECT_ROOT_DIR
#define PROJECT_ROOT_DIR "."
#endif

bool SessionSetupView::Render(Session& session, SDL_Renderer* renderer) {
    float scale = ImGui::GetIO().FontGlobalScale;

    if (ImGui::BeginChild("SessionSetupRegion", ImVec2(0, 0), false)) {
        ImVec2 totalAvail = ImGui::GetContentRegionAvail();
        
        float bottomReserved = showValidationError ? (70.0f * scale) : (45.0f * scale);
        float contentHeight = totalAvail.y - bottomReserved;
        float leftWidth = totalAvail.x * 0.5f;

        if (ImGui::BeginChild("LeftColumn", ImVec2(leftWidth, contentHeight), false)) {
            
            if (ImGui::BeginChild("GeneralInfo", ImVec2(0, 110.0f * scale), true)) {
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "SESSION CONFIGURATION");
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));

                ImGui::BeginGroup();
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Session Name:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - (10.0f * scale));
                ImGui::InputText("##SessionName", session.sessionName, IM_ARRAYSIZE(session.sessionName));
                
                ImGui::Text("Date:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - (10.0f * scale));
                ImGui::InputText("##SessionDate", session.sessionDate, IM_ARRAYSIZE(session.sessionDate));
                ImGui::EndGroup();
            }
            ImGui::EndChild();

            if (ImGui::BeginChild("TrackInfo", ImVec2(0, 0), true)) {
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "TRACK LAYOUT & GPS OVERLAY");
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));

                float rightControlsWidth = 120.0f * scale;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - rightControlsWidth);
                
                const char* trackPreview = session.selectedTrack.name.empty() ? "Select Track..." : session.selectedTrack.name.c_str();
                
                if (ImGui::BeginCombo("##TrackCombo", trackPreview)) {
                    // Aggiorniamo la lista da disco appena il menu a tendina si apre
                    if (ImGui::IsWindowAppearing()) {
                        availableTracks = TrackSerializer::GetAvailableTracks();
                        memset(trackSearchBuffer, 0, sizeof(trackSearchBuffer));
                    }

                    // La barra di ricerca integrata
                    ImGui::InputTextWithHint("##SearchCombo", "Search...", trackSearchBuffer, sizeof(trackSearchBuffer));
                    ImGui::Separator();

                    std::string searchStr(trackSearchBuffer);
                    std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

                    for (size_t i = 0; i < availableTracks.size(); ++i) {
                        if (!searchStr.empty()) {
                            std::string tNameLower = availableTracks[i];
                            std::transform(tNameLower.begin(), tNameLower.end(), tNameLower.begin(), ::tolower);
                            if (tNameLower.find(searchStr) == std::string::npos) continue;
                        }

                        bool isSelected = (availableTracks[i] == session.selectedTrack.name);
                        if (ImGui::Selectable(availableTracks[i].c_str(), isSelected)) {
                            session.selectedTrack.name = availableTracks[i];
                            
                            // Carichiamo immediatamente i dati della pista per l'anteprima
                            std::string path = std::string(PROJECT_ROOT_DIR) + "/assets/tracks/" + availableTracks[i] + ".json";
                            TrackSerializer::LoadTrack(path, session.selectedTrack);
                            needsMapCentering = true;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                
                ImGui::SameLine(0.0f, 6.0f * scale);
                
                if (ImGui::Button("Tracks Manager", ImVec2(110.0f * scale, 0.0f))) {
                    trackManagerModal.Open(); 
                }

                ImGui::Dummy(ImVec2(0.0f, 10.0f * scale));
                
                ImVec2 canvasSize = ImGui::GetContentRegionAvail();
                canvasSize.y = std::max(20.0f * scale, canvasSize.y - (5.0f * scale));
                ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                ImVec2 canvasCenter(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);
                
                if (needsMapCentering && !session.selectedTrack.name.empty()) {
                    double cLat, cLon;
                    float cZoom;
                    GeoMath::CalculateTrackCenterAndZoom(session.selectedTrack, canvasSize, 0.15f, cLat, cLon, cZoom);
                    mapView.CenterOn(cLat, cLon, cZoom);
                    needsMapCentering = false;
                }

                if (session.selectedTrack.name.empty()) {
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        canvasPos, 
                        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
                        ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.13f, 1.0f))
                    );
                    ImGui::GetWindowDrawList()->AddRect(
                        canvasPos, 
                        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), 
                        ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f))
                    );
                    ImGui::InvisibleButton("##TrackCanvas", canvasSize);
                } else {
                    bool isMapHovered, isMapActive;
                    // Mappa interattiva solo per la navigazione (pan/zoom)
                    mapView.RenderBaseMap(renderer, canvasPos, canvasSize, true, isMapHovered, isMapActive);

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

                    // Versione ultra-leggera per disegnare i limiti senza snap al cursore o frecce direzionali enormi
                    auto drawBound = [&](const auto& bound, ImU32 color) {
                        for (size_t i = 1; i < bound.size(); ++i) {
                            ImVec2 p1 = mapView.GetScreenPos(bound[i-1].lat, bound[i-1].lon, canvasCenter);
                            ImVec2 p2 = mapView.GetScreenPos(bound[i].lat, bound[i].lon, canvasCenter);
                            drawList->AddLine(p1, p2, color, 2.0f);
                        }
                    };

                    drawBound(session.selectedTrack.limits.leftBound, IM_COL32(255, 50, 50, 255));
                    drawBound(session.selectedTrack.limits.rightBound, IM_COL32(50, 50, 255, 255));

                    for (const auto& fence : session.selectedTrack.fences) {
                        ImVec2 p1 = mapView.GetScreenPos(fence.p1.lat, fence.p1.lon, canvasCenter);
                        ImVec2 p2 = mapView.GetScreenPos(fence.p2.lat, fence.p2.lon, canvasCenter);
                        ImU32 fColor = (fence.type == TrackFenceType::FinishLine) ? IM_COL32(50, 255, 50, 255) : IM_COL32(255, 150, 0, 255);
                        drawList->AddLine(p1, p2, fColor, 4.0f);
                    }

                    drawList->PopClipRect();
                    mapView.RenderUIControls(canvasPos, canvasSize); 
                }
            }
            ImGui::EndChild();

        }
        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::BeginChild("RightColumn", ImVec2(0, contentHeight), true)) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "SESSION DRIVERS & DATA SOURCES");
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));

            float mgrBtnWidth = 120.0f * scale;
            float spacing = 6.0f * scale;

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - mgrBtnWidth - spacing);
            
            if (ImGui::BeginCombo("##DriverCombo", "Add a driver...")) {
                for (size_t i = 0; i < availableDrivers.size(); ++i) {
                    
                    bool alreadyAdded = false;
                    for (const auto& d : session.drivers) {
                        if (d.name == availableDrivers[i]) {
                            alreadyAdded = true;
                            break;
                        }
                    }

                    if (alreadyAdded) {
                        ImGui::BeginDisabled();
                        ImGui::Selectable((availableDrivers[i] + " (Added)").c_str(), false);
                        ImGui::EndDisabled();
                    } else {
                        if (ImGui::Selectable(availableDrivers[i].c_str(), false)) {
                            session.drivers.push_back({availableDrivers[i], "", ""});
                        }
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine(0.0f, spacing);

            if (ImGui::Button("Drivers Manager", ImVec2(mgrBtnWidth, 0.0f))) {
                // Logica futura per aprire il Drivers Manager
            }

            ImGui::Dummy(ImVec2(0.0f, 10.0f * scale));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));

            if (ImGui::BeginChild("DriversListRegion", ImGui::GetContentRegionAvail(), false)) {
                for (size_t i = 0; i < session.drivers.size(); ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    
                    ImGui::Text("%s", session.drivers[i].name.c_str());
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - (20.0f * scale));
                    if (ImGui::Button("X##remove", ImVec2(24.0f * scale, 20.0f * scale))) {
                        session.drivers.erase(session.drivers.begin() + i);
                        ImGui::PopID();
                        break;
                    }

                    ImGui::TextDisabled("Telemetry:");
                    ImGui::SameLine();
                    if (session.drivers[i].telemetryPath.empty()) {
                        if (ImGui::Button("Attach CSV...##telemetry", ImVec2(160.0f * scale, 0.0f))) {
                            session.drivers[i].telemetryPath = "simulated_data.csv";
                        }
                    } else {
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", session.drivers[i].telemetryPath.c_str());
                        ImGui::SameLine();
                        if (ImGui::Button("X##rem_telemetry", ImVec2(20.0f * scale, 20.0f * scale))) {
                            session.drivers[i].telemetryPath.clear();
                        }
                    }

                    ImGui::TextDisabled("Video File:");
                    ImGui::SameLine();
                    if (session.drivers[i].videoPath.empty()) {
                        if (ImGui::Button("Attach MP4...##video", ImVec2(160.0f * scale, 0.0f))) {
                            session.drivers[i].videoPath = "simulated_cam.mp4";
                        }
                    } else {
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", session.drivers[i].videoPath.c_str());
                        ImGui::SameLine();
                        if (ImGui::Button("X##rem_video", ImVec2(20.0f * scale, 20.0f * scale))) {
                            session.drivers[i].videoPath.clear();
                        }
                    }

                    ImGui::Dummy(ImVec2(0.0f, 8.0f * scale));
                    ImGui::Separator();
                    ImGui::Dummy(ImVec2(0.0f, 8.0f * scale));

                    ImGui::PopID();
                }
                ImGui::EndChild();
            }
        }
        ImGui::EndChild();

        if (showValidationError) {
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "Error: Please add at least one driver with a telemetry file to proceed.");
        }

        ImGui::Dummy(ImVec2(0.0f, 2.0f * scale));

        float buttonWidth = 200.0f * scale;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttonWidth);

        if (ImGui::Button("Launch Analysis Workspace", ImVec2(buttonWidth, 28.0f * scale))) {
            bool hasValidData = false;
            for (const auto& d : session.drivers) {
                if (!d.telemetryPath.empty()) {
                    hasValidData = true;
                    break;
                }
            }

            if (strlen(session.sessionName) > 0 && !session.drivers.empty() && hasValidData) {
                showValidationError = false;
                
                trackManagerModal.Render(renderer);
                ImGui::EndChild();
                return true; 
            } else {
                showValidationError = true;
            }
        }
    }
    
   
    trackManagerModal.Render(renderer);
    
    ImGui::EndChild();
    return false;
}