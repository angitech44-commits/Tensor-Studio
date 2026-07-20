#include "SessionSetupModal.hpp"
#include <imgui.h>
#include <cstring>
#include <algorithm>

void SessionSetupModal::Render(Session& session, bool& isProjectOpen, int& activeTab) {
    ImGuiIO& io = ImGui::GetIO();

    ImU32 dimColor = ImGui::GetColorU32(ImGuiCol_ModalWindowDimBg);
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(0, 0), 
        ImVec2(io.DisplaySize.x, io.DisplaySize.y - 45.0f), 
        dimColor
    );
    
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::InvisibleButton("##WorkspaceBlocker", ImVec2(io.DisplaySize.x, io.DisplaySize.y - 45.0f));

    ImVec2 modalSize = ImVec2(io.DisplaySize.x * 0.85f, (io.DisplaySize.y - 45.0f) * 0.85f);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, (io.DisplaySize.y - 45.0f) * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(modalSize, ImGuiCond_Appearing);

    ImGuiWindowFlags dialogFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("Project Setup - Session Configuration", nullptr, dialogFlags)) {
        ImGui::BeginGroup();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Session Name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(250.0f);
        ImGui::InputText("##SessionName", session.sessionName, IM_ARRAYSIZE(session.sessionName));
        
        ImGui::SameLine(0.0f, 30.0f);
        ImGui::Text("Date:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        ImGui::InputText("##SessionDate", session.sessionDate, IM_ARRAYSIZE(session.sessionDate));
        ImGui::EndGroup();

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 5.0f));

        if (ImGui::BeginChild("MainSetupContentWindow", ImVec2(0.0f, -50.0f), false)) {
            if (ImGui::BeginTable("SetupColumnsTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImGui::GetContentRegionAvail())) {
                ImGui::TableSetupColumn("TrackSelectionCol", ImGuiTableColumnFlags_WidthStretch, 0.5f);
                ImGui::TableSetupColumn("DriverSelectionCol", ImGuiTableColumnFlags_WidthStretch, 0.5f);

                ImGui::TableNextRow();
                
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "TRACK LAYOUT & GPS OVERLAY");
                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                ImGui::Text("Track:");
                ImGui::SameLine();
                
                float rightControlsWidth = 110.0f;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - rightControlsWidth);
                
                if (!isTrackEditMode) {
                    if (ImGui::BeginCombo("##TrackCombo", session.selectedTrack.name.c_str())) {
                        for (size_t i = 0; i < availableTracks.size(); ++i) {
                            bool isSelected = (availableTracks[i] == session.selectedTrack.name);
                            if (ImGui::Selectable(availableTracks[i].c_str(), isSelected)) {
                                session.selectedTrack.name = availableTracks[i];
                            }
                            if (isSelected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::Separator();
                        if (ImGui::Selectable("+ New Track...")) {
                            isTrackEditMode = true;
                            originalTrackName = "";
                            trackNameBuffer[0] = '\0';
                        }
                        ImGui::EndCombo();
                    }
                } else {
                    ImGui::InputTextWithHint("##TrackInput", "Enter track name...", trackNameBuffer, IM_ARRAYSIZE(trackNameBuffer));
                }
                
                ImGui::SameLine(0.0f, 6.0f);
                if (!isTrackEditMode) {
                    if (ImGui::Button("Edit##editTrack", ImVec2(50.0f, 0.0f))) {
                        if (!session.selectedTrack.name.empty()) {
                            isTrackEditMode = true;
                            originalTrackName = session.selectedTrack.name;
                            strncpy(trackNameBuffer, session.selectedTrack.name.c_str(), sizeof(trackNameBuffer) - 1);
                            trackNameBuffer[sizeof(trackNameBuffer) - 1] = '\0';
                        }
                    }
                } else {
                    if (ImGui::Button("Save##saveTrack", ImVec2(50.0f, 0.0f))) {
                        if (strlen(trackNameBuffer) > 0) {
                            if (originalTrackName.empty()) {
                                availableTracks.push_back(trackNameBuffer);
                            } else {
                                for (auto& t : availableTracks) {
                                    if (t == originalTrackName) {
                                        t = trackNameBuffer;
                                        break;
                                    }
                                }
                            }
                            session.selectedTrack.name = trackNameBuffer;
                            isTrackEditMode = false;
                        }
                    }
                }
                
                ImGui::SameLine(0.0f, 4.0f);
                if (ImGui::Button("Del##delTrack", ImVec2(50.0f, 0.0f))) {
                    if (isTrackEditMode) {
                        isTrackEditMode = false;
                    } else {
                        for (auto it = availableTracks.begin(); it != availableTracks.end(); ) {
                            if (*it == session.selectedTrack.name) {
                                it = availableTracks.erase(it);
                                break;
                            } else {
                                ++it;
                            }
                        }
                        if (!availableTracks.empty()) {
                            session.selectedTrack.name = availableTracks[0];
                        } else {
                            session.selectedTrack.name = "";
                        }
                    }
                }

                ImGui::Dummy(ImVec2(0.0f, 10.0f));
                
                ImVec2 canvasSize = ImGui::GetContentRegionAvail();
                canvasSize.y = std::max(20.0f, canvasSize.y - 5.0f);
                
                ImVec2 canvasPos = ImGui::GetCursorScreenPos();
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

                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "SESSION DRIVERS & DATA SOURCES");
                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
                ImGui::InputTextWithHint("##NewDriverInput", "Driver Name...", newDriverInput, IM_ARRAYSIZE(newDriverInput));
                ImGui::SameLine();
                if (ImGui::Button("Add Driver##btn", ImVec2(90.0f, 0.0f))) {
                    if (strlen(newDriverInput) > 0) {
                        session.drivers.push_back({std::string(newDriverInput), "", ""});
                        newDriverInput[0] = '\0';
                    }
                }

                ImGui::Dummy(ImVec2(0.0f, 10.0f));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                if (ImGui::BeginChild("DriversListRegion", ImGui::GetContentRegionAvail(), false)) {
                    for (size_t i = 0; i < session.drivers.size(); ++i) {
                        ImGui::PushID(static_cast<int>(i));
                        
                        ImGui::Text("%s", session.drivers[i].name.c_str());
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
                        if (ImGui::Button("X##remove", ImVec2(24.0f, 20.0f))) {
                            session.drivers.erase(session.drivers.begin() + i);
                            ImGui::PopID();
                            break;
                        }

                        ImGui::TextDisabled("Telemetry:");
                        ImGui::SameLine();
                        if (session.drivers[i].telemetryPath.empty()) {
                            if (ImGui::Button("Attach CSV...##telemetry", ImVec2(160.0f, 0.0f))) {
                                session.drivers[i].telemetryPath = "simulated_data.csv";
                            }
                        } else {
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", session.drivers[i].telemetryPath.c_str());
                            ImGui::SameLine();
                            if (ImGui::Button("X##rem_telemetry", ImVec2(20.0f, 20.0f))) {
                                session.drivers[i].telemetryPath.clear();
                            }
                        }

                        ImGui::TextDisabled("Video File:");
                        ImGui::SameLine();
                        if (session.drivers[i].videoPath.empty()) {
                            if (ImGui::Button("Attach MP4...##video", ImVec2(160.0f, 0.0f))) {
                                session.drivers[i].videoPath = "simulated_cam.mp4";
                            }
                        } else {
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", session.drivers[i].videoPath.c_str());
                            ImGui::SameLine();
                            if (ImGui::Button("X##rem_video", ImVec2(20.0f, 20.0f))) {
                                session.drivers[i].videoPath.clear();
                            }
                        }

                        ImGui::Dummy(ImVec2(0.0f, 8.0f));
                        ImGui::Separator();
                        ImGui::Dummy(ImVec2(0.0f, 8.0f));

                        ImGui::PopID();
                    }
                    ImGui::EndChild();
                }

                ImGui::EndTable();
            }
            ImGui::EndChild();
        }

        if (showValidationError) {
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "Error: Please add at least one driver with a telemetry file to proceed.");
        }

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        if (ImGui::Button("Launch Analysis Workspace", ImVec2(200, 28))) {
            bool hasValidData = false;
            for (const auto& d : session.drivers) {
                if (!d.telemetryPath.empty()) {
                    hasValidData = true;
                    break;
                }
            }

            if (strlen(session.sessionName) > 0 && !session.drivers.empty() && hasValidData) {
                isProjectOpen = true;
                showValidationError = false;
            } else {
                showValidationError = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 28))) {
            activeTab = -1;
            showValidationError = false;
        }

        ImGui::End();
    }
}