#include "SessionSetupView.hpp"
#include <imgui.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include "../../io/TrackSerializer.hpp"
#include "../../utils/GeoMath.hpp"

#ifndef PROJECT_ROOT_DIR
#define PROJECT_ROOT_DIR "."
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool SessionSetupView::Render(Session& session, SDL_Renderer* renderer) {
    float scale = ImGui::GetIO().FontGlobalScale;

    if (ImGui::BeginChild("SessionSetupRegion", ImVec2(0, 0), false)) {
        ImVec2 totalAvail = ImGui::GetContentRegionAvail();
        
        float bottomReserved = showValidationError ? (70.0f * scale) : (45.0f * scale);
        float contentHeight = totalAvail.y - bottomReserved;
        float leftWidth = totalAvail.x * 0.5f;

        if (ImGui::BeginChild("LeftColumn", ImVec2(leftWidth, contentHeight), false)) {
            
            // Calcolo per rendere la mappa quadrata (Altezza = Larghezza della colonna)
            float desiredMapHeight = leftWidth;
            float minDriversBoxHeight = 250.0f * scale; // Altezza minima per la lista piloti
            
            if (contentHeight - desiredMapHeight < minDriversBoxHeight) {
                desiredMapHeight = contentHeight - minDriversBoxHeight;
            }
            float topHeight = contentHeight - desiredMapHeight - (8.0f * scale); // Tolgo 8px per la spaziatura tra i child
            
            // --- BOX PILOTI E DATI (A SINISTRA IN ALTO) ---
            if (ImGui::BeginChild("DriversInfo", ImVec2(0, topHeight), true)) {
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "SESSION DRIVERS & DATA SOURCES");
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));

                float mgrBtnWidth = 120.0f * scale;
                float spacing = 6.0f * scale;

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - mgrBtnWidth - spacing);
                
                if (ImGui::BeginCombo("##DriverCombo", "Add a driver...")) {
                    if (ImGui::IsWindowAppearing()) {
                        availableDrivers = DriverSerializer::GetAvailableDrivers();
                    }

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
                    driverManagerModal.Open();
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
                            if (referenceDriver == session.drivers[i].name) referenceDriver = "";
                            
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
                }
                ImGui::EndChild(); 
            }
            ImGui::EndChild();

            // --- BOX MAPPA (ORA DINAMICAMENTE QUADRATA IN BASSO) ---
            if (ImGui::BeginChild("TrackInfo", ImVec2(0, 0), true)) {
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "TRACK LAYOUT & GPS OVERLAY");
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));

                float rightControlsWidth = 120.0f * scale;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - rightControlsWidth);
                
                const char* trackPreview = session.selectedTrack.name.empty() ? "Select Track..." : session.selectedTrack.name.c_str();
                
                if (ImGui::BeginCombo("##TrackCombo", trackPreview)) {
                    if (ImGui::IsWindowAppearing()) {
                        availableTracks = TrackSerializer::GetAvailableTracks();
                        memset(trackSearchBuffer, 0, sizeof(trackSearchBuffer));
                    }

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
                    mapView.RenderBaseMap(renderer, canvasPos, canvasSize, true, isMapHovered, isMapActive);

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

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

                        ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
                        float screenAngle = -fence.directionHeading; 
                        float arrowLen = 20.0f;
                        ImVec2 arrowEnd(mid.x + std::cos(screenAngle) * arrowLen, mid.y + std::sin(screenAngle) * arrowLen);
                        
                        ImU32 arrowColor = IM_COL32(255, 255, 255, 255);
                        drawList->AddLine(mid, arrowEnd, arrowColor, 3.0f);
                        
                        float headLen = 8.0f;
                        float angle1 = screenAngle + M_PI * 0.85f;
                        float angle2 = screenAngle - M_PI * 0.85f;
                        drawList->AddLine(arrowEnd, ImVec2(arrowEnd.x + std::cos(angle1) * headLen, arrowEnd.y + std::sin(angle1) * headLen), arrowColor, 3.0f);
                        drawList->AddLine(arrowEnd, ImVec2(arrowEnd.x + std::cos(angle2) * headLen, arrowEnd.y + std::sin(angle2) * headLen), arrowColor, 3.0f);
                    }

                    drawList->PopClipRect();
                    mapView.RenderUIControls(canvasPos, canvasSize); 
                }
            }
            ImGui::EndChild();

        }
        ImGui::EndChild();

        ImGui::SameLine();

        // --- BOX CONFIGURAZIONE E TIMELINE (A DESTRA A TUTTA ALTEZZA) ---
        if (ImGui::BeginChild("RightColumn", ImVec2(0, contentHeight), true)) {
            
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "SESSION CONFIGURATION");
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));

            ImGui::BeginGroup();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Session Name:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - (10.0f * scale));
            ImGui::InputTextWithHint("##SessionName", "Auto-generated from telemetry...", session.sessionName, IM_ARRAYSIZE(session.sessionName));
            
            ImGui::Text("Date:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - (10.0f * scale));
            ImGui::InputTextWithHint("##SessionDate", "Auto-detected...", session.sessionDate, IM_ARRAYSIZE(session.sessionDate));
            ImGui::EndGroup();

            ImGui::Dummy(ImVec2(0.0f, 10.0f * scale));
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "SESSION TIMELINE");
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));

            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Reference Driver:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - (10.0f * scale));
            
            const char* refPreview = referenceDriver.empty() ? "Select a reference driver..." : referenceDriver.c_str();
            if (ImGui::BeginCombo("##RefDriver", refPreview)) {
                for (const auto& d : session.drivers) {
                    bool isSelected = (referenceDriver == d.name);
                    if (ImGui::Selectable(d.name.c_str(), isSelected)) {
                        referenceDriver = d.name;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));

            if (ImGui::BeginChild("GlobalSplitsRegion", ImVec2(0, 0), false)) {
                for (size_t s = 0; s < globalSplits.size(); ++s) {
                    ImGui::PushID(static_cast<int>(s));
                    
                    float rowWidth = ImGui::GetContentRegionAvail().x;
                    float startX = ImGui::GetCursorPosX();
                    
                    // Blocco Inizio (Sinistra)
                    ImGui::BeginGroup();
                    ImGui::TextDisabled("Start Lap:");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(50.0f * scale);
                    ImGui::InputInt("##Start", &globalSplits[s].startLap, 0, 0);
                    ImGui::Checkbox("Outlap", &globalSplits[s].hasOutlap);
                    ImGui::EndGroup();

                    // Blocco Tipo Sessione (Centro Esatto)
                    ImGui::SameLine(startX + (rowWidth * 0.5f) - (64.0f * scale));
                    ImGui::BeginGroup();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f * scale);
                    bool isFP = (globalSplits[s].type == SplitType::FreePractice);
                    bool isQuali = (globalSplits[s].type == SplitType::Qualifying);
                    bool isRace = (globalSplits[s].type == SplitType::Race);

                    if (isFP) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
                    if (ImGui::Button("FP", ImVec2(38.0f * scale, 0))) globalSplits[s].type = SplitType::FreePractice;
                    if (isFP) ImGui::PopStyleColor();

                    ImGui::SameLine(0, 2.0f);
                    if (isQuali) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.5f, 0.2f, 1.0f));
                    if (ImGui::Button("Quali", ImVec2(45.0f * scale, 0))) globalSplits[s].type = SplitType::Qualifying;
                    if (isQuali) ImGui::PopStyleColor();

                    ImGui::SameLine(0, 2.0f);
                    if (isRace) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    if (ImGui::Button("Race", ImVec2(45.0f * scale, 0))) globalSplits[s].type = SplitType::Race;
                    if (isRace) ImGui::PopStyleColor();
                    ImGui::EndGroup();

                    // Blocco Fine (Destra, appena prima del tasto X)
                    float endBlockWidth = 115.0f * scale; 
                    float xButtonOffset = 34.0f * scale;  
                    ImGui::SameLine(startX + rowWidth - endBlockWidth - xButtonOffset);
                    
                    ImGui::BeginGroup();
                    ImGui::TextDisabled("End Lap:");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(50.0f * scale);
                    ImGui::InputInt("##End", &globalSplits[s].endLap, 0, 0);
                    ImGui::Checkbox("Inlap", &globalSplits[s].hasInlap);
                    ImGui::EndGroup();

                    // Tasto X (Estrema Destra)
                    ImGui::SameLine(startX + rowWidth - (24.0f * scale));
                    if (ImGui::Button("X##rem_split", ImVec2(24.0f * scale, 24.0f * scale))) {
                        globalSplits.erase(globalSplits.begin() + s);
                        ImGui::PopID();
                        break; 
                    }
                    
                    if (globalSplits[s].endLap < globalSplits[s].startLap) globalSplits[s].endLap = globalSplits[s].startLap;

                    ImGui::Dummy(ImVec2(0.0f, 2.0f * scale));

                    // Barra visiva dinamica integrata per i giri
                    int totalLaps = globalSplits[s].endLap - globalSplits[s].startLap + 1;
                    if (totalLaps > 0) {
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        float w = ImGui::GetContentRegionAvail().x;
                        float h = 12.0f * scale; 
                        float rectW = w / totalLaps;
                        
                        int validLaps = totalLaps;
                        
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        for (int l = 0; l < totalLaps; ++l) {
                            ImU32 color;
                            
                            if (l == 0 && globalSplits[s].hasOutlap) {
                                color = IM_COL32(50, 150, 255, 255); // Blu (Outlap)
                            } 
                            else if (l == totalLaps - 1 && globalSplits[s].hasInlap && totalLaps > 1) {
                                color = IM_COL32(255, 70, 70, 255); // Rosso (Inlap)
                            } 
                            else {
                                color = IM_COL32(50, 200, 50, 255); // Verde (Giro Valido)
                            }
                            
                            drawList->AddRectFilled(ImVec2(p.x + l * rectW, p.y), ImVec2(p.x + (l+1) * rectW - 1.0f, p.y + h), color);
                        }
                        ImGui::Dummy(ImVec2(0, h + (2.0f * scale)));
                        
                        if (globalSplits[s].hasOutlap && totalLaps > 0) validLaps--;
                        if (globalSplits[s].hasInlap && totalLaps > 1) validLaps--;

                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Total Laps: %d (Valid Laps: %d)", totalLaps, validLaps);
                    }
                    
                    ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));
                    ImGui::Separator();
                    ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));
                    ImGui::PopID();
                }

                ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));
                if (ImGui::Button("+ Add Session Split", ImVec2(-1, 0))) {
                    int nextLap = globalSplits.empty() ? 1 : globalSplits.back().endLap + 1;
                    globalSplits.push_back({nextLap, nextLap + 8, true, true, SplitType::FreePractice});
                }
            }
            ImGui::EndChild();
            
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
                driverManagerModal.Render();
                ImGui::EndChild();
                return true; 
            } else {
                showValidationError = true;
            }
        }
    }
    
    trackManagerModal.Render(renderer);
    driverManagerModal.Render();
    
    ImGui::EndChild();
    return false;
}