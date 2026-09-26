#include "SessionSetupView.hpp"
#include <imgui.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include "../../io/TrackSerializer.hpp"
#include "../../utils/GeoMath.hpp"
#include "../../io/FileParser.hpp"
#include "../../analysis/SpatialResampler.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Identifica se siamo su Desktop
#if (defined(_WIN32) || defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))) && !defined(__ANDROID__)
    #define IS_DESKTOP_PLATFORM
#endif

void SessionSetupView::UpdateRefTelemetry(const std::string& path, const Track& track) {
    cachedRefLaps.clear();
    cachedRefDriverLaps = 12; 
    if (path.empty() || track.fences.empty()) return;
    
    std::string filename = path.substr(path.find_last_of("/\\") + 1);
    std::string baseName = filename.substr(0, filename.find_last_of("."));
    std::string digitsOnly;
    for (char c : baseName) { if (std::isdigit(c)) digitsOnly += c; }
    double sbt = 0.0;
    if (!digitsOnly.empty()) { try { sbt = std::stod(digitsOnly); } catch (...) {} }

    FileParser parser;
    parser.ParseCSV(path);
    SpatialResampler resampler;
    auto fused = resampler.Resample(parser.imuData, parser.gnssData);
    LapSplitter splitter;
    auto laps = splitter.SplitLaps(fused, track, sbt);
    
    GlobalTelemetryCache[path] = { fused, laps, sbt, parser.imuData, parser.gnssData };
    cachedRefLaps = laps;
    
    if (!cachedRefLaps.empty()) {
        cachedRefDriverLaps = cachedRefLaps.back().lapNumber;
    }
}

int SessionSetupView::CalculateEndLapByTime(int startLap, int durationMinutes) {
    if (cachedRefLaps.empty() || startLap < 1 || startLap > cachedRefLaps.size()) return startLap;
    
    double targetDurationSec = durationMinutes * 60.0;
    double accumulatedTime = 0.0;
    
    int endLap = startLap;
    for (size_t i = startLap - 1; i < cachedRefLaps.size(); ++i) {
        accumulatedTime += cachedRefLaps[i].timeSeconds;
        endLap = cachedRefLaps[i].lapNumber;
        
        if (accumulatedTime >= targetDurationSec) {
            break;
        }
    }
    return endLap;
}

bool SessionSetupView::Render(Session& session, SDL_Renderer* renderer) {
    float scale = ImGui::GetIO().FontGlobalScale;
    bool launchRequested = false;

    // Variabile di stato per Android (0 = Pista, 1 = Piloti, 2 = Split)
    static int mobileStep = 0;

    // Condizioni della Macchina a Stati
    bool hasTrack = !session.selectedTrack.name.empty();
    bool hasTelemetry = false;
    for (const auto& d : session.drivers) {
        if (!d.telemetryPath.empty()) {
            hasTelemetry = true;
            break;
        }
    }

    // ====================================================================================
    // BLOCCO 1: LOGICA PISTA
    // ====================================================================================
    auto RenderTrackSection = [&](float height) {
        if (ImGui::BeginChild("TrackInfo", ImVec2(0, height), true)) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "1. TRACK LAYOUT");
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
                        std::string path = TrackSerializer::GetTracksDirectory() + availableTracks[i] + ".json";
                        TrackSerializer::LoadTrack(path, session.selectedTrack);
                        needsMapCentering = true;
                        lastRefTelemetryPath = ""; 
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            
            ImGui::SameLine(0.0f, 6.0f * scale);
            if (ImGui::Button("Manage", ImVec2(110.0f * scale, 0.0f))) {
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
                ImGui::GetWindowDrawList()->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.13f, 1.0f)));
                ImGui::GetWindowDrawList()->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f)));
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
                }

                drawList->PopClipRect();
                mapView.RenderUIControls(canvasPos, canvasSize); 
            }
        }
        ImGui::EndChild();
    };

    // ====================================================================================
    // BLOCCO 2: LOGICA PILOTI
    // ====================================================================================
    auto RenderDriversSection = [&](float height) {
        if (ImGui::BeginChild("DriversInfo", ImVec2(0, height), true)) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "2. SESSION DRIVERS");
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));

            float mgrBtnWidth = 120.0f * scale;
            float spacing = 6.0f * scale;

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - mgrBtnWidth - spacing);
            if (ImGui::BeginCombo("##DriverCombo", "Add a driver...")) {
                if (ImGui::IsWindowAppearing()) availableDrivers = DriverSerializer::GetAvailableDrivers();
                for (size_t i = 0; i < availableDrivers.size(); ++i) {
                    bool alreadyAdded = false;
                    for (const auto& d : session.drivers) { if (d.name == availableDrivers[i]) { alreadyAdded = true; break; } }
                    if (alreadyAdded) {
                        ImGui::BeginDisabled();
                        ImGui::Selectable((availableDrivers[i] + " (Added)").c_str(), false);
                        ImGui::EndDisabled();
                    } else {
                        if (ImGui::Selectable(availableDrivers[i].c_str(), false)) session.drivers.push_back({availableDrivers[i], "", ""});
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine(0.0f, spacing);
            if (ImGui::Button("Manage", ImVec2(mgrBtnWidth, 0.0f))) {
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
                        if (session.referenceDriver == session.drivers[i].name) {
                            session.referenceDriver = "";
                            lastRefTelemetryPath = "";
                            session.splits.clear();
                        }
                        session.drivers.erase(session.drivers.begin() + i);
                        ImGui::PopID();
                        break;
                    }

                    ImGui::TextDisabled("Telemetry:");
                    ImGui::SameLine();
                    if (session.drivers[i].telemetryPath.empty()) {
                        if (ImGui::Button("Link Telemetry...##telemetry", ImVec2(160.0f * scale, 0.0f))) {
                            telemetryImportModal.Open(&session.drivers[i].telemetryPath, session.selectedTrack);
                        }
                    } else {
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", session.drivers[i].telemetryPath.c_str());
                        ImGui::SameLine();
                        if (ImGui::Button("X##rem_telemetry", ImVec2(20.0f * scale, 20.0f * scale))) {
                            if (session.referenceDriver == session.drivers[i].name) {
                                session.referenceDriver = "";
                                lastRefTelemetryPath = "";
                                session.splits.clear();
                            }
                            session.drivers[i].telemetryPath.clear();
                        }
                    }

                    ImGui::TextDisabled("Video File:");
                    ImGui::SameLine();
                    if (session.drivers[i].videoPath.empty()) {
                        if (ImGui::Button("Attach MP4...##video", ImVec2(160.0f * scale, 0.0f))) session.drivers[i].videoPath = "simulated_cam.mp4";
                    } else {
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", session.drivers[i].videoPath.c_str());
                        ImGui::SameLine();
                        if (ImGui::Button("X##rem_video", ImVec2(20.0f * scale, 20.0f * scale))) session.drivers[i].videoPath.clear();
                    }
                    ImGui::Dummy(ImVec2(0.0f, 8.0f * scale));
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }
            ImGui::EndChild(); 
        }
        ImGui::EndChild();
    };

    // ====================================================================================
    // BLOCCO 3: LOGICA SPLIT E LANCIO
    // ====================================================================================
    auto RenderSplitsSection = [&](float height) {
        if (ImGui::BeginChild("RightColumn", ImVec2(0, height), true)) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "3. SESSION TIMELINE");
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 5.0f * scale));

            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Ref Driver:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - (10.0f * scale));
            
            bool refValid = false;
            for (const auto& d : session.drivers) {
                if (d.name == session.referenceDriver && !d.telemetryPath.empty()) {
                    refValid = true; 
                    if (d.telemetryPath != lastRefTelemetryPath) {
                        lastRefTelemetryPath = d.telemetryPath;
                        UpdateRefTelemetry(d.telemetryPath, session.selectedTrack);
                        session.splits.clear();
                        
                        SessionSplit defaultSplit;
                        defaultSplit.startLap = 1;
                        defaultSplit.endLap = cachedRefDriverLaps;
                        defaultSplit.hasOutlap = true;
                        defaultSplit.hasInlap = true;
                        defaultSplit.type = SplitType::FreePractice;
                        defaultSplit.durationMode = SplitDurationMode::Laps;
                        session.splits.push_back(defaultSplit);
                    }
                    break;
                }
            }
            
            if (!refValid && !session.referenceDriver.empty()) {
                session.referenceDriver = "";
                lastRefTelemetryPath = "";
                session.splits.clear();
            }

            const char* refPreview = session.referenceDriver.empty() ? "Select Ref Driver..." : session.referenceDriver.c_str();
            if (ImGui::BeginCombo("##RefDriver", refPreview)) {
                for (const auto& d : session.drivers) {
                    if (!d.telemetryPath.empty()) { 
                        bool isSelected = (session.referenceDriver == d.name);
                        if (ImGui::Selectable(d.name.c_str(), isSelected)) {
                            if (session.referenceDriver != d.name) {
                                session.referenceDriver = d.name;
                                lastRefTelemetryPath = d.telemetryPath;
                                UpdateRefTelemetry(d.telemetryPath, session.selectedTrack);
                                session.splits.clear();
                                
                                SessionSplit defaultSplit;
                                defaultSplit.startLap = 1;
                                defaultSplit.endLap = cachedRefDriverLaps;
                                defaultSplit.hasOutlap = true;
                                defaultSplit.hasInlap = true;
                                defaultSplit.type = SplitType::FreePractice;
                                defaultSplit.durationMode = SplitDurationMode::Laps;
                                session.splits.push_back(defaultSplit);
                            }
                        }
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Dummy(ImVec2(0.0f, 10.0f * scale));

            ImGuiID advSettingsID = ImGui::GetID("Advanced Settings");
            bool isAdvOpen = ImGui::GetStateStorage()->GetBool(advSettingsID, false);
            float reservedBottomHeight = (isAdvOpen ? 95.0f : 35.0f) * scale;
            float splitsRegionHeight = std::max(20.0f * scale, ImGui::GetContentRegionAvail().y - reservedBottomHeight);

            if (session.referenceDriver.empty()) {
                if (ImGui::BeginChild("EmptySplitsRegion", ImVec2(0, splitsRegionHeight), false)) {
                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    const char* msg = "Select a Reference Driver to setup timeline.";
                    ImVec2 textSize = ImGui::CalcTextSize(msg);
                    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (avail.x - textSize.x) * 0.5f, ImGui::GetCursorPosY() + 40.0f * scale));
                    ImGui::TextDisabled("%s", msg);
                }
                ImGui::EndChild();
            } else {
                if (ImGui::BeginChild("GlobalSplitsRegion", ImVec2(0, splitsRegionHeight), false)) {
                    if (cachedRefDriverLaps > 0) {
                        ImGui::TextDisabled("Preview (%d Laps Total)", cachedRefDriverLaps);
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        float w = ImGui::GetContentRegionAvail().x;
                        float h = 18.0f * scale;
                        float rectW = w / cachedRefDriverLaps;
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        
                        for (int l = 1; l <= cachedRefDriverLaps; ++l) {
                            ImU32 color = IM_COL32(40, 40, 40, 255); 
                            for (const auto& split : session.splits) {
                                if (l >= split.startLap && l <= split.endLap) {
                                    if (split.type == SplitType::FreePractice) color = IM_COL32(50, 150, 220, 255);
                                    else if (split.type == SplitType::Qualifying) color = IM_COL32(220, 140, 50, 255);
                                    else color = IM_COL32(220, 60, 60, 255);
                                    break; 
                                }
                            }
                            drawList->AddRectFilled(ImVec2(p.x + (l-1) * rectW, p.y), ImVec2(p.x + l * rectW - 1.0f, p.y + h), color);
                        }
                        ImGui::Dummy(ImVec2(0, h + 15.0f * scale));
                    }

                    for (size_t s = 0; s < session.splits.size(); ++s) {
                        ImGui::PushID(static_cast<int>(s));
                        SessionSplit& split = session.splits[s];
                        if (split.durationMode == SplitDurationMode::Time) split.endLap = CalculateEndLapByTime(split.startLap, split.durationMinutes);

                        // Calcolo dinamico adattivo: La card si ingrandisce o si rimpicciolisce in base al contenuto disegnato
                        float cardHeight = (split.durationMode == SplitDurationMode::Laps ? 140.0f : 165.0f) * scale;

                        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
                        if (ImGui::BeginChild("SplitCard", ImVec2(0, cardHeight), true, ImGuiWindowFlags_NoScrollbar)) {
                            bool isFP = (split.type == SplitType::FreePractice);
                            bool isQuali = (split.type == SplitType::Qualifying);
                            bool isRace = (split.type == SplitType::Race);

                            if (isFP) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
                            if (ImGui::Button("FP", ImVec2(40.0f * scale, 0))) split.type = SplitType::FreePractice;
                            if (isFP) ImGui::PopStyleColor();

                            ImGui::SameLine(0, 2.0f);
                            if (isQuali) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.5f, 0.2f, 1.0f));
                            if (ImGui::Button("Quali", ImVec2(50.0f * scale, 0))) split.type = SplitType::Qualifying;
                            if (isQuali) ImGui::PopStyleColor();

                            ImGui::SameLine(0, 2.0f);
                            if (isRace) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                            if (ImGui::Button("Race", ImVec2(50.0f * scale, 0))) split.type = SplitType::Race;
                            if (isRace) ImGui::PopStyleColor();

                            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24.0f * scale);
                            if (ImGui::Button("X##rem", ImVec2(24.0f * scale, 24.0f * scale))) {
                                session.splits.erase(session.splits.begin() + s);
                                ImGui::EndChild(); ImGui::PopStyleVar(); ImGui::PopID();
                                break; 
                            }
                            
                            ImGui::Separator();
                            int mode = (int)split.durationMode;
                            ImGui::RadioButton("Laps", &mode, 0); ImGui::SameLine(0, 15.0f * scale); ImGui::RadioButton("Time", &mode, 1);
                            split.durationMode = (SplitDurationMode)mode;
                            
                            if (split.durationMode == SplitDurationMode::Laps) {
                                ImGui::DragIntRange2("Start/End", &split.startLap, &split.endLap, 0.2f, 1, cachedRefDriverLaps);
                                if (split.startLap < 1) split.startLap = 1;
                                if (split.endLap > cachedRefDriverLaps) split.endLap = cachedRefDriverLaps;
                            } else {
                                ImGui::SliderInt("Start Lap", &split.startLap, 1, cachedRefDriverLaps);
                                ImGui::SliderInt("Minutes", &split.durationMinutes, 1, 120);
                            }

                            // CHECKBOX REINSERITE
                            ImGui::Dummy(ImVec2(0, 2.0f * scale));
                            ImGui::Checkbox("Has Outlap", &split.hasOutlap);
                            ImGui::SameLine(0, 15.0f * scale);
                            ImGui::Checkbox("Has Inlap", &split.hasInlap);
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleVar();
                        ImGui::PopID();
                    }

                    if (ImGui::Button("+ Add Split", ImVec2(-1, 0))) {
                        if (session.splits.empty()) {
                            session.splits.push_back({1, cachedRefDriverLaps, true, true, SplitType::FreePractice, SplitDurationMode::Laps, 0, 0, 0});
                        } else {
                            SessionSplit& lastSplit = session.splits.back();
                            if (lastSplit.endLap < cachedRefDriverLaps) {
                                session.splits.push_back({lastSplit.endLap + 1, cachedRefDriverLaps, true, true, SplitType::FreePractice, SplitDurationMode::Laps, 0, 0, 0});
                            }
                        }
                    }
                }
                ImGui::EndChild();
            }

            bool advOpen = ImGui::CollapsingHeader("Advanced Settings");
            ImGui::GetStateStorage()->SetBool(advSettingsID, advOpen); 
            if (advOpen) {
                ImGui::InputTextWithHint("##SessionName", "Session Name...", session.sessionName, IM_ARRAYSIZE(session.sessionName));
                ImGui::InputTextWithHint("##SessionDate", "Date...", session.sessionDate, IM_ARRAYSIZE(session.sessionDate));
            }
        }
        ImGui::EndChild();
    };

    // ====================================================================================
    // RENDERIZZAZIONE PIATTAFORMA-SPECIFICA
    // ====================================================================================
    if (ImGui::BeginChild("SessionSetupRegion", ImVec2(0, 0), false)) {
        ImVec2 totalAvail = ImGui::GetContentRegionAvail();

#ifdef IS_DESKTOP_PLATFORM
        // --- LAYOUT DESKTOP (FIANCO A FIANCO CON EFFETTO NEBBIA) ---
        float bottomReserved = showValidationError ? (70.0f * scale) : (45.0f * scale);
        float contentHeight = totalAvail.y - bottomReserved;
        float leftWidth = totalAvail.x * 0.5f;

        if (ImGui::BeginChild("LeftColumnContainer", ImVec2(leftWidth, contentHeight), false)) {
            float desiredMapHeight = leftWidth;
            float minDriversBoxHeight = 250.0f * scale; 
            if (contentHeight - desiredMapHeight < minDriversBoxHeight) desiredMapHeight = contentHeight - minDriversBoxHeight;
            
            // 1. Pista (Sempre attiva)
            RenderTrackSection(desiredMapHeight);
            ImGui::Dummy(ImVec2(0.0f, 8.0f * scale));

            // 2. Piloti (Attiva solo se Pista selezionata, altrimenti Nebbia)
            ImVec2 driversMin = ImGui::GetCursorScreenPos();
            ImGui::BeginDisabled(!hasTrack);
            RenderDriversSection(0); // Prende lo spazio rimanente
            ImGui::EndDisabled();
            
            if (!hasTrack) {
                ImVec2 driversMax = ImVec2(driversMin.x + leftWidth, driversMin.y + (contentHeight - desiredMapHeight));
                ImGui::GetWindowDrawList()->AddRectFilled(driversMin, driversMax, IM_COL32(20, 20, 20, 200));
                
                const char* msg = "Please select a Track to unlock Drivers";
                ImVec2 textSize = ImGui::CalcTextSize(msg);
                ImGui::GetWindowDrawList()->AddText(ImVec2(driversMin.x + (leftWidth - textSize.x)*0.5f, driversMin.y + 50.0f*scale), IM_COL32(255, 255, 255, 255), msg);
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // 3. Split (Attiva solo se Piloti + Telemetria, altrimenti Nebbia)
        ImVec2 splitsMin = ImGui::GetCursorScreenPos();
        ImGui::BeginDisabled(!hasTelemetry);
        RenderSplitsSection(contentHeight);
        ImGui::EndDisabled();

        if (!hasTelemetry) {
            ImVec2 splitsMax = ImVec2(splitsMin.x + totalAvail.x - leftWidth, splitsMin.y + contentHeight);
            ImGui::GetWindowDrawList()->AddRectFilled(splitsMin, splitsMax, IM_COL32(20, 20, 20, 200));
            
            const char* msg = "Attach Telemetry to unlock Timeline";
            ImVec2 textSize = ImGui::CalcTextSize(msg);
            ImGui::GetWindowDrawList()->AddText(ImVec2(splitsMin.x + ((totalAvail.x - leftWidth) - textSize.x)*0.5f, splitsMin.y + 50.0f*scale), IM_COL32(255, 255, 255, 255), msg);
        }

        // Bottone di Lancio (Sempre in fondo al Desktop)
        ImGui::Dummy(ImVec2(0.0f, 2.0f * scale));
        float buttonWidth = 200.0f * scale;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttonWidth);
        
        ImGui::BeginDisabled(!hasTelemetry || session.splits.empty());
        if (ImGui::Button("Launch Analysis Workspace", ImVec2(buttonWidth, 28.0f * scale))) {
            launchRequested = true;
        }
        ImGui::EndDisabled();

#else
        // --- LAYOUT MOBILE (PAGINAZIONE A WIZARD) ---
        float navBarHeight = 50.0f * scale;
        float contentHeight = totalAvail.y - navBarHeight;

        // Renderizziamo solo il blocco corrispondente allo step attuale
        if (mobileStep == 0) {
            RenderTrackSection(contentHeight);
        } 
        else if (mobileStep == 1) {
            RenderDriversSection(contentHeight);
        } 
        else if (mobileStep == 2) {
            RenderSplitsSection(contentHeight);
        }

        // Barra di navigazione Mobile in basso
        ImGui::Dummy(ImVec2(0, 5.0f * scale));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5.0f * scale));

        if (mobileStep > 0) {
            if (ImGui::Button("< Back", ImVec2(100.0f * scale, 35.0f * scale))) {
                mobileStep--;
            }
        } else {
            ImGui::Dummy(ImVec2(100.0f * scale, 35.0f * scale)); // Spaziatore
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - (120.0f * scale));

        if (mobileStep == 0) {
            ImGui::BeginDisabled(!hasTrack);
            if (ImGui::Button("Next >", ImVec2(120.0f * scale, 35.0f * scale))) mobileStep++;
            ImGui::EndDisabled();
        } 
        else if (mobileStep == 1) {
            ImGui::BeginDisabled(!hasTelemetry);
            if (ImGui::Button("Next >", ImVec2(120.0f * scale, 35.0f * scale))) mobileStep++;
            ImGui::EndDisabled();
        } 
        else if (mobileStep == 2) {
            ImGui::BeginDisabled(!hasTelemetry || session.splits.empty());
            if (ImGui::Button("LAUNCH!", ImVec2(120.0f * scale, 35.0f * scale))) {
                launchRequested = true;
            }
            ImGui::EndDisabled();
        }
#endif

        // ====================================================================================
        // LOGICA DI AVVIO (Eseguita indipendentemente se Desktop o Mobile)
        // ====================================================================================
        if (launchRequested) {
            if (strlen(session.sessionName) > 0 && !session.drivers.empty() && hasTelemetry) {
                showValidationError = false;
                
                for (const auto& d : session.drivers) {
                    if (!d.telemetryPath.empty()) {
                        std::string filename = d.telemetryPath.substr(d.telemetryPath.find_last_of("/\\") + 1);
                        std::string baseName = filename.substr(0, filename.find_last_of("."));
                        std::string digitsOnly;
                        for (char c : baseName) { if (std::isdigit(c)) digitsOnly += c; }
                        double sbt = 0.0;
                        if (!digitsOnly.empty()) { try { sbt = std::stod(digitsOnly); } catch (...) {} }

                        FileParser parser;
                        parser.ParseCSV(d.telemetryPath);
                        SpatialResampler resampler;
                        auto fused = resampler.Resample(parser.imuData, parser.gnssData);
                        LapSplitter splitter;
                        auto laps = splitter.SplitLaps(fused, session.selectedTrack, sbt);
                        
                        GlobalTelemetryCache[d.telemetryPath] = { fused, laps, sbt, parser.imuData, parser.gnssData };
                    }
                }

                if (!session.referenceDriver.empty()) {
                    std::string refPath = "";
                    for (const auto& d : session.drivers) {
                        if (d.name == session.referenceDriver) refPath = d.telemetryPath;
                    }
                    if (!refPath.empty() && GlobalTelemetryCache.count(refPath)) {
                        const auto& refCache = GlobalTelemetryCache[refPath];
                        for (auto& split : session.splits) {
                            if (!refCache.laps.empty()) {
                                int sIdx = std::clamp(split.startLap, 1, (int)refCache.laps.size()) - 1;
                                int eIdx = std::clamp(split.endLap, 1, (int)refCache.laps.size()) - 1;
                                
                                split.absoluteStartTime = refCache.baseTime + refCache.laps[sIdx].startTime;
                                split.absoluteEndTime = refCache.baseTime + refCache.laps[eIdx].endTime;
                            }
                        }
                    }
                }
                
                trackManagerModal.Render(renderer);
                driverManagerModal.Render();
                ImGui::EndChild();
                return true; 
            } else {
                showValidationError = true;
            }
        }
    }
    
    // Le modali devono SEMPRE renderizzare fuori dalle disabilitazioni/nebbia
    trackManagerModal.Render(renderer);
    driverManagerModal.Render();
    telemetryImportModal.Render(renderer);
    
    ImGui::EndChild();
    return false;
}