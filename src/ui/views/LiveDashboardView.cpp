#include "LiveDashboardView.hpp"
#include "../../io/TrackSerializer.hpp"
#include "../../utils/GeoMath.hpp"
#include <imgui.h>
#include <implot.h>
#include <cmath>
#include <algorithm>
#include <SDL3/SDL.h>

#if (defined(_WIN32) || defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))) && !defined(__ANDROID__)
    #define IS_DESKTOP_PLATFORM
    #include <SDL3/SDL_opengl.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- HELPER FUNZIONI ---
static void RenderCenteredText(const char* text, float scale = 1.0f, ImU32 color = IM_COL32_WHITE) {
    if (scale != 1.0f) ImGui::SetWindowFontScale(scale);
    
    ImVec2 textSize = ImGui::CalcTextSize(text);
    float availWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (availWidth - textSize.x) * 0.5f));
    
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Text("%s", text);
    ImGui::PopStyleColor();
    
    if (scale != 1.0f) ImGui::SetWindowFontScale(1.0f);
}

static void ToggleFullscreen(SDL_Renderer* renderer) {
    SDL_Window* win = nullptr;
#ifdef IS_DESKTOP_PLATFORM
    win = SDL_GL_GetCurrentWindow();
#else
    if (renderer) win = SDL_GetRenderWindow(renderer);
#endif
    if (win) {
        Uint32 flags = SDL_GetWindowFlags(win);
        bool isFullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
        SDL_SetWindowFullscreen(win, !isFullscreen);
    }
}

LiveDashboardView::LiveDashboardView() {}

LiveDashboardView::~LiveDashboardView() {
    processor.Stop();
}

std::string LiveDashboardView::FormatTime(float seconds) {
    if (seconds <= 0.0f || seconds > 900.0f) return "--:--.---";
    int m = static_cast<int>(seconds) / 60;
    float s = seconds - (m * 60);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d:%06.3f", m, s);
    return std::string(buf);
}

void LiveDashboardView::DrawGBubble(ImVec2 center, float radius, float accX, float accY) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    drawList->AddCircleFilled(center, radius, IM_COL32(30, 30, 30, 255), 64);
    drawList->AddCircle(center, radius, IM_COL32(100, 100, 100, 255), 64, std::max(2.0f, radius * 0.02f));
    drawList->AddCircle(center, radius * 0.5f, IM_COL32(70, 70, 70, 255), 64, std::max(1.0f, radius * 0.01f));
    
    drawList->AddLine(ImVec2(center.x - radius, center.y), ImVec2(center.x + radius, center.y), IM_COL32(70, 70, 70, 255));
    drawList->AddLine(ImVec2(center.x, center.y - radius), ImVec2(center.x, center.y + radius), IM_COL32(70, 70, 70, 255));

    float dotX = center.x + (std::clamp(accX, -2.0f, 2.0f) / 2.0f) * radius;
    float dotY = center.y + (std::clamp(accY, -2.0f, 2.0f) / 2.0f) * radius;

    drawList->AddCircleFilled(ImVec2(dotX, dotY), std::max(10.0f, radius * 0.12f), IM_COL32(255, 50, 50, 255));
    drawList->AddCircle(ImVec2(dotX, dotY), std::max(12.0f, radius * 0.14f), IM_COL32(255, 255, 255, 255), 32, std::max(2.0f, radius * 0.02f));
}

void LiveDashboardView::UpdateDebugPlotData() {
    std::vector<IMUFrame> imuHist;
    std::vector<GNSSFrame> gnssHist;
    processor.GetHistory(imuHist, gnssHist);

    imuTimes.clear(); accX.clear(); accY.clear(); accZ.clear();
    gyroX.clear(); gyroY.clear(); gyroZ.clear();
    for (const auto& f : imuHist) {
        double tSec = f.time / 1000000.0; 
        imuTimes.push_back(tSec);
        accX.push_back(f.accX); accY.push_back(f.accY); accZ.push_back(f.accZ);
        gyroX.push_back(f.gyroX); gyroY.push_back(f.gyroY); gyroZ.push_back(f.gyroZ);
    }

    gnssTimes.clear(); speed.clear(); altitude.clear();
    for (const auto& f : gnssHist) {
        double tSec = f.time / 1000000.0;
        gnssTimes.push_back(tSec);
        speed.push_back(f.speed * 0.0036f); 
        altitude.push_back(f.alt);
    }
}

void LiveDashboardView::Render(SDL_Renderer* renderer) {
    ImGuiIO& io = ImGui::GetIO();
    float scale = io.FontGlobalScale;
    ImVec2 avail = ImGui::GetContentRegionAvail();

    static float currentLiveZoom = 18.0f;
    static float lastCurTime = 0.0f;
    static int completedLaps = 0;
    
    static float emaX = 0.0f, emaY = 0.0f;
    static bool emaInit = false;

    static double smoothLat = 0.0;
    static double smoothLon = 0.0;
    static float smoothHead = 0.0f;

    static float myBestTime = 9999.0f;
    static float myPrevTime = 0.0f;
    static bool justScoredPB = false;

    static float freezeTimer = 0.0f;
    static float freezeTimeValue = 0.0f;
    static float currentDelta = 0.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::BeginChild("LiveDashMaster", avail, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (currentState == LiveState::Setup) {
        if (ImGui::BeginTable("SetupTable", 2)) {
            ImGui::TableSetupColumn("Settings", ImGuiTableColumnFlags_WidthFixed, avail.x * 0.4f);
            ImGui::TableSetupColumn("MapPreview", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Dummy(ImVec2(0, 40.0f * scale));
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "TELEMETRY DASHBOARD SETUP");
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 10.0f * scale));

            ImGui::TextDisabled("Listening Port: 8080 (UDP)");
            ImGui::Dummy(ImVec2(0, 10.0f * scale));

            ImGui::Text("Select Track:");
            const char* trackPreview = selectedTrack.name.empty() ? "Choose Track..." : selectedTrack.name.c_str();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120.0f * scale);
            if (ImGui::BeginCombo("##TrackLiveCombo", trackPreview)) {
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
                    if (ImGui::Selectable(availableTracks[i].c_str(), availableTracks[i] == selectedTrack.name)) {
                        selectedTrack.name = availableTracks[i];
                        std::string path = TrackSerializer::GetTracksDirectory() + availableTracks[i] + ".json";
                        TrackSerializer::LoadTrack(path, selectedTrack);
                        
                        if (!selectedTrack.limits.leftBound.empty()) {
                            cachePointIndex = 0;
                        }
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            if (ImGui::Button("Manage Tracks")) trackManagerModal.Open();

            ImGui::Dummy(ImVec2(0, 20.0f * scale));
            ImGui::TextDisabled("Map preview will automatically download \nand cache map tiles for offline use.");
            ImGui::Dummy(ImVec2(0, 20.0f * scale));
            
            ImGui::BeginDisabled(cachePointIndex >= 0);
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            if (ImGui::Button("CONNECT DASHBOARD", ImVec2(-1, 40.0f * scale))) {
                if (!selectedTrack.name.empty() && processor.Start(selectedTrack)) {
                    showDebugView = false;
                    currentState = LiveState::Waiting;
                    completedLaps = 0;
                    lastCurTime = 0.0f;
                    myBestTime = 9999.0f;
                    myPrevTime = 0.0f;
                    freezeTimer = 0.0f;
                    justScoredPB = false;
                    currentDelta = 0.0f;
                    emaInit = false;
                    smoothLat = 0.0;
                }
            }
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0, 10.0f * scale));

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.4f, 0.2f, 1.0f));
            if (ImGui::Button("CONNECT DEBUG", ImVec2(-1, 40.0f * scale))) {
                if (!selectedTrack.name.empty() && processor.Start(selectedTrack)) {
                    showDebugView = true;
                    currentState = LiveState::Waiting;
                    completedLaps = 0;
                    lastCurTime = 0.0f;
                    myBestTime = 9999.0f;
                    myPrevTime = 0.0f;
                    freezeTimer = 0.0f;
                    justScoredPB = false;
                    currentDelta = 0.0f;
                    emaInit = false;
                    smoothLat = 0.0;
                }
            }
            ImGui::PopStyleColor();
            
            ImGui::Dummy(ImVec2(0, 10.0f * scale));

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("TOGGLE FULLSCREEN", ImVec2(-1, 40.0f * scale))) {
                ToggleFullscreen(renderer);
            }
            ImGui::PopStyleColor();

            ImGui::EndDisabled();

            ImGui::TableSetColumnIndex(1);
            if (!selectedTrack.name.empty()) {
                if (cachePointIndex >= 0) {
                    int percent = (cachePointIndex * 100) / (int)selectedTrack.limits.leftBound.size();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Downloading High-Res Map Cache... %d%%", percent);
                } else {
                    ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Offline Cache Ready");
                }
                
                if (ImGui::BeginChild("MapCacheRegion", ImVec2(0, 0), true)) {
                    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
                    
                    if (cachePointIndex >= 0 && cachePointIndex < selectedTrack.limits.leftBound.size()) {
                        mapView.CenterOn(selectedTrack.limits.leftBound[cachePointIndex].lat, 
                                         selectedTrack.limits.leftBound[cachePointIndex].lon, 18.0f);
                        cachePointIndex += 2; 
                        if (cachePointIndex >= selectedTrack.limits.leftBound.size()) cachePointIndex = -1; 
                    } else {
                        double cLat, cLon; float cZoom;
                        GeoMath::CalculateTrackCenterAndZoom(selectedTrack, canvasSize, 0.15f, cLat, cLon, cZoom);
                        mapView.CenterOn(cLat, cLon, cZoom);
                    }
                    
                    bool hov, act;
                    mapView.RenderBaseMap(renderer, canvasPos, canvasSize, true, hov, act);

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);
                    ImVec2 canvasCenter(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);
                    
                    for (const auto& fence : selectedTrack.fences) {
                        ImVec2 p1 = mapView.GetScreenPos(fence.p1.lat, fence.p1.lon, canvasCenter);
                        ImVec2 p2 = mapView.GetScreenPos(fence.p2.lat, fence.p2.lon, canvasCenter);
                        drawList->AddLine(p1, p2, IM_COL32(255, 150, 0, 255), 4.0f);
                    }
                    drawList->PopClipRect();
                    mapView.RenderUIControls(canvasPos, canvasSize);
                }
                ImGui::EndChild();
            }
            ImGui::EndTable();
        }
        trackManagerModal.Render(renderer); 
    }
    else if (currentState == LiveState::Waiting) {
        if (processor.HasData()) currentState = LiveState::Active;

        ImGui::SetCursorPos(ImVec2(avail.x * 0.4f, avail.y * 0.4f));
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "WAITING FOR TENSOR DATA...");
        ImGui::TextDisabled("Make sure the device is on and sending to Port 8080.");
        
        ImGui::Dummy(ImVec2(0, 20.0f * scale));
        if (ImGui::Button("Cancel", ImVec2(200.0f * scale, 30.0f * scale))) {
            processor.Stop();
            currentState = LiveState::Setup;
        }
    }
    else if (currentState == LiveState::Active) {
        IMUFrame curImu;
        GNSSFrame curGnss;
        processor.GetLatestData(curImu, curGnss);

        float curTime, processorPrevTime, processorBestTime;
        processor.GetTiming(curTime, processorPrevTime, processorBestTime);

        // INTEGRAZIONE 100Hz HARDWARE
        float highResTime = curTime;
        if (curImu.time >= curGnss.time && curGnss.time > 0) {
            highResTime += static_cast<float>((curImu.time - curGnss.time) / 1000000.0);
        }

        // Controllo Traguardo e Logica PB/Outlap
        if (highResTime < lastCurTime - 5.0f) {
            completedLaps++; 
            myPrevTime = processorPrevTime; 

            justScoredPB = false;
            if (completedLaps > 1) { 
                if (myPrevTime > 0.0f && (myBestTime == 9999.0f || myPrevTime < myBestTime)) {
                    myBestTime = myPrevTime;
                    justScoredPB = true;
                }
            }

            freezeTimer = 3.0f; // Congela lo schermo per 3 secondi esatti
            freezeTimeValue = myPrevTime;
        }
        lastCurTime = highResTime;

        if (freezeTimer > 0.0f) {
            freezeTimer -= io.DeltaTime;
        }

        // --- CALCOLO DELTA SPAZIALE IN TEMPO REALE ---
        auto bestTraj = processor.GetBestTrajectory();
        if (completedLaps >= 2 && !bestTraj.empty() && curGnss.lat != 0.0) {
            double minDist = 999999999.0;
            int minIdx = -1;
            // Scansione veloce ogni 2 punti per trovare la posizione più vicina nel best lap
            for (size_t i = 0; i < bestTraj.size(); i += 2) {
                double dLat = bestTraj[i].lat - curGnss.lat;
                double dLon = bestTraj[i].lon - curGnss.lon;
                double dist = dLat*dLat + dLon*dLon;
                if (dist < minDist) {
                    minDist = dist;
                    minIdx = i;
                }
            }
            if (minIdx >= 0) {
                double bestPtTimeLap = (bestTraj[minIdx].time - bestTraj.front().time) / 1000000.0;
                float rawDelta = highResTime - static_cast<float>(bestPtTimeLap);
                
                // Smoother sul delta per renderlo leggibile
                if (std::abs(rawDelta - currentDelta) > 2.0f) currentDelta = rawDelta;
                else currentDelta += (rawDelta - currentDelta) * 10.0f * io.DeltaTime;
            }
        } else {
            currentDelta = 0.0f;
        }

        // --- LOGICA G-BUBBLE: EMA sui Raw ---
        if (!emaInit) { 
            emaX = curImu.accX; 
            emaY = curImu.accY; 
            emaInit = true; 
        } else {
            emaX = 0.015f * curImu.accX + (1.0f - 0.015f) * emaX;
            emaY = 0.015f * curImu.accY + (1.0f - 0.015f) * emaY;
        }

        // Interpolazione spaziale fluida per la mappa
        if (smoothLat == 0.0 && curGnss.lat != 0.0) {
            smoothLat = curGnss.lat;
            smoothLon = curGnss.lon;
            smoothHead = curGnss.head;
        } else if (curGnss.lat != 0.0) {
            smoothLat += (curGnss.lat - smoothLat) * 0.15;
            smoothLon += (curGnss.lon - smoothLon) * 0.15;
            
            float headDiff = curGnss.head - smoothHead;
            while (headDiff < -180.0f) headDiff += 360.0f;
            while (headDiff >  180.0f) headDiff -= 360.0f;
            smoothHead += headDiff * 0.15f;
        }

        bool isDebugThisFrame = showDebugView;

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("DISCONNECT & SAVE", ImVec2(200.0f * scale, 35.0f * scale))) {
            processor.Stop();
            currentState = LiveState::Setup;
        }
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        if (ImGui::Button(isDebugThisFrame ? "SWITCH TO DASHBOARD" : "SWITCH TO DEBUG", ImVec2(200.0f * scale, 35.0f * scale))) {
            showDebugView = !isDebugThisFrame;
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("TOGGLE FULLSCREEN", ImVec2(200.0f * scale, 35.0f * scale))) {
            ToggleFullscreen(renderer);
        }
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0, 10.0f * scale));

        ImGuiTableFlags layoutFlags = ImGuiTableFlags_BordersInnerV;
        if (!isDebugThisFrame) layoutFlags |= ImGuiTableFlags_SizingStretchProp;

        if (ImGui::BeginTable("DashboardLayout", isDebugThisFrame ? 2 : 3, layoutFlags)) {
            
            if (isDebugThisFrame) {
                // MODALITÀ DEBUG 
                ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthFixed, avail.x * 0.25f);
                ImGui::TableSetupColumn("Graphs", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "TELEMETRY DEBUG");
                ImGui::Separator();
                
                if (ImGui::TreeNodeEx("Live Values", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("Lat: %.6f", curGnss.lat);
                    ImGui::Text("Lon: %.6f", curGnss.lon);
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "Speed: %.1f km/h", curGnss.speed * 0.0036f);
                    ImGui::Dummy(ImVec2(0, 5.0f * scale));
                    ImGui::Text("Acc X: %6.3f G", curImu.accX);
                    ImGui::Text("Acc Y: %6.3f G", curImu.accY);
                    ImGui::Text("Acc Z: %6.3f G", curImu.accZ);
                    ImGui::TreePop();
                }

                ImGui::TableSetColumnIndex(1);
                UpdateDebugPlotData();

                double latestTime = 0.0;
                if (!imuTimes.empty()) latestTime = std::max(latestTime, imuTimes.back());
                if (!gnssTimes.empty()) latestTime = std::max(latestTime, gnssTimes.back());

                double windowSizeSec = 10.0;
                double xMin = latestTime > windowSizeSec ? latestTime - windowSizeSec : 0.0;
                double xMax = latestTime > windowSizeSec ? latestTime : windowSizeSec;

                if (ImPlot::BeginSubplots("Live Telemetry", 3, 1, ImVec2(-1, -1), ImPlotSubplotFlags_LinkAllX | ImPlotSubplotFlags_NoTitle)) {
                    if (ImPlot::BeginPlot("Acceleration (G)")) {
                        ImPlot::SetupAxes("Time (s)", "G", ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
                        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax, ImGuiCond_Always);
                        if (!imuTimes.empty()) {
                            ImPlot::PlotLine("Acc X", imuTimes.data(), accX.data(), imuTimes.size());
                            ImPlot::PlotLine("Acc Y", imuTimes.data(), accY.data(), imuTimes.size());
                            ImPlot::PlotLine("Acc Z", imuTimes.data(), accZ.data(), imuTimes.size());
                        }
                        ImPlot::EndPlot();
                    }
                    if (ImPlot::BeginPlot("Gyroscope (deg/s)")) {
                        ImPlot::SetupAxes("Time (s)", "deg/s", ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
                        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax, ImGuiCond_Always);
                        if (!imuTimes.empty()) {
                            ImPlot::PlotLine("Gyro X", imuTimes.data(), gyroX.data(), imuTimes.size());
                            ImPlot::PlotLine("Gyro Y", imuTimes.data(), gyroY.data(), imuTimes.size());
                            ImPlot::PlotLine("Gyro Z", imuTimes.data(), gyroZ.data(), imuTimes.size());
                        }
                        ImPlot::EndPlot();
                    }
                    if (ImPlot::BeginPlot("Speed (km/h)")) {
                        ImPlot::SetupAxes("Time (s)", "km/h", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
                        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax, ImGuiCond_Always);
                        if (!gnssTimes.empty()) ImPlot::PlotLine("Speed", gnssTimes.data(), speed.data(), gnssTimes.size());
                        ImPlot::EndPlot();
                    }
                    ImPlot::EndSubplots();
                }

            } else {
                
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

                // --- MODALITÀ DASHBOARD ---
                ImGui::TableSetupColumn("Col1", ImGuiTableColumnFlags_WidthStretch, 0.25f);
                ImGui::TableSetupColumn("Col2", ImGuiTableColumnFlags_WidthStretch, 0.50f);
                ImGui::TableSetupColumn("Col3", ImGuiTableColumnFlags_WidthStretch, 0.25f);
                ImGui::TableNextRow();

                ImDrawList* drawList = ImGui::GetWindowDrawList();

                // ====================================================================
                // COLONNA 1: VELOCITÀ E BOLLA G 
                // ====================================================================
                ImGui::TableSetColumnIndex(0);

                float col1Width = ImGui::GetContentRegionAvail().x;
                float col1Height = ImGui::GetContentRegionAvail().y;

                auto DrawAdaptiveTextCol1 = [&](const char* text, float targetWidthPct, float targetHeightPct, ImU32 color) {
                    ImVec2 baseSize = ImGui::CalcTextSize(text);
                    if (baseSize.x <= 0.0f || baseSize.y <= 0.0f) return;

                    float scaleX = (col1Width * targetWidthPct) / baseSize.x;
                    float scaleY = (col1Height * targetHeightPct) / baseSize.y;
                    
                    float finalScale = std::min(scaleX, scaleY);
                    if (finalScale < 0.1f) finalScale = 0.1f; 

                    RenderCenteredText(text, finalScale, color);
                };

                ImGui::Dummy(ImVec2(0, col1Height * 0.05f)); 
                DrawAdaptiveTextCol1("SPEED", 0.35f, 0.05f, IM_COL32(150, 150, 150, 255)); 

                char speedText[32];
                snprintf(speedText, sizeof(speedText), "%.0f", curGnss.speed * 0.0036f);
                DrawAdaptiveTextCol1(speedText, 0.85f, 0.25f, IM_COL32(255, 255, 255, 255)); 

                DrawAdaptiveTextCol1("km/h", 0.30f, 0.05f, IM_COL32(150, 150, 150, 255)); 
                ImGui::Dummy(ImVec2(0, col1Height * 0.05f)); 
                DrawAdaptiveTextCol1("G-FORCE", 0.45f, 0.05f, IM_COL32(150, 150, 150, 255)); 
                ImGui::Dummy(ImVec2(0, col1Height * 0.03f)); 

                float remainingYCol1 = ImGui::GetContentRegionAvail().y;
                float maxDiamX = col1Width * 0.90f;
                float maxDiamY = remainingYCol1 * 0.95f; 
                float bubbleRadius = std::min(maxDiamX, maxDiamY) * 0.5f;

                if (bubbleRadius < 30.0f * scale) bubbleRadius = 30.0f * scale;

                ImVec2 gCenter = ImVec2(
                    ImGui::GetCursorScreenPos().x + (col1Width * 0.5f), 
                    ImGui::GetCursorScreenPos().y + bubbleRadius + (5.0f * scale)
                );

                DrawGBubble(gCenter, bubbleRadius, emaX, emaY);
                ImGui::Dummy(ImVec2(col1Width, remainingYCol1));

                // ====================================================================
                // COLONNA 2: TEMPI E DELTA BAR
                // ====================================================================
                ImGui::TableSetColumnIndex(1);
                
                float col2Width = ImGui::GetContentRegionAvail().x;
                float col2Height = ImGui::GetContentRegionAvail().y;

                auto DrawAdaptiveTextCol2 = [&](const char* text, float targetWidthPct, float targetHeightPct, ImU32 color) {
                    float cellWidth = ImGui::GetContentRegionAvail().x;
                    ImVec2 baseSize = ImGui::CalcTextSize(text);
                    if (baseSize.x <= 0.0f || baseSize.y <= 0.0f) return;

                    float scaleX = (cellWidth * targetWidthPct) / baseSize.x;
                    float scaleY = (col2Height * targetHeightPct) / baseSize.y;
                    
                    float finalScale = std::min(scaleX, scaleY);
                    if (finalScale < 0.1f) finalScale = 0.1f;

                    RenderCenteredText(text, finalScale, color);
                };

                ImGui::Dummy(ImVec2(0, col2Height * 0.05f));

                if (ImGui::BeginTable("LapsTable", 2)) {
                    ImGui::TableNextRow();
                    
                    ImGui::TableSetColumnIndex(0);
                    DrawAdaptiveTextCol2("BEST LAP", 0.60f, 0.05f, IM_COL32(200, 150, 50, 255)); 
                    std::string bestStr = (completedLaps > 1 && myBestTime < 9999.0f) ? FormatTime(myBestTime) : "--:--.---";
                    DrawAdaptiveTextCol2(bestStr.c_str(), 0.90f, 0.15f, IM_COL32(255, 255, 255, 255));
                    
                    ImGui::TableSetColumnIndex(1);
                    DrawAdaptiveTextCol2("PREV LAP", 0.60f, 0.05f, IM_COL32(150, 150, 150, 255));
                    std::string prevStr = (completedLaps >= 1 && myPrevTime > 0.0f) ? (completedLaps == 1 ? "OUTLAP" : FormatTime(myPrevTime)) : "--:--.---";
                    DrawAdaptiveTextCol2(prevStr.c_str(), 0.90f, 0.15f, IM_COL32(255, 255, 255, 255));
                    
                    ImGui::EndTable(); 
                }

                ImGui::Dummy(ImVec2(0, col2Height * 0.05f)); 
                DrawAdaptiveTextCol2("CURRENT TIME", 0.60f, 0.05f, IM_COL32(150, 150, 150, 255)); 
                ImGui::Dummy(ImVec2(0, col2Height * 0.01f)); 

                char curTimeText[32];
                ImU32 timeColor = IM_COL32(255, 255, 255, 255);

                if (freezeTimer > 0.0f) {
                    if (completedLaps == 1) {
                        snprintf(curTimeText, sizeof(curTimeText), "OUTLAP");
                        timeColor = IM_COL32(150, 150, 150, 255);
                    } else {
                        int cm = static_cast<int>(freezeTimeValue) / 60;
                        float cs = freezeTimeValue - (cm * 60);
                        snprintf(curTimeText, sizeof(curTimeText), "%d:%06.3f", cm, cs);
                        timeColor = justScoredPB ? IM_COL32(50, 255, 50, 255) : IM_COL32(255, 200, 50, 255);
                    }
                } else {
                    int cm = static_cast<int>(highResTime) / 60;
                    float cs = highResTime - (cm * 60);
                    snprintf(curTimeText, sizeof(curTimeText), "%d:%04.1f", cm, cs);
                }

                DrawAdaptiveTextCol2(curTimeText, 0.95f, 0.25f, timeColor); 
                ImGui::Dummy(ImVec2(0, col2Height * 0.03f)); 

                // --- SETTORE DELTA ---
                DrawAdaptiveTextCol2("DELTA", 0.35f, 0.05f, IM_COL32(150, 150, 150, 255)); 
                ImGui::Dummy(ImVec2(0, col2Height * 0.01f)); 

                char deltaText[32];
                if (completedLaps >= 2 && !bestTraj.empty()) {
                    snprintf(deltaText, sizeof(deltaText), "%+0.2f", currentDelta);
                    ImU32 deltaColor = (currentDelta <= 0.0f) ? IM_COL32(50, 255, 50, 255) : IM_COL32(255, 20, 20, 255);
                    
                    DrawAdaptiveTextCol2(deltaText, 0.95f, 0.25f, deltaColor); 
                    
                    ImGui::Dummy(ImVec2(0, col2Height * 0.01f)); 
                    
                    ImVec2 barPos = ImGui::GetCursorScreenPos();
                    float barWidth = col2Width; 
                    float barHeight = col2Height * 0.06f; 
                    
                    drawList->AddRectFilled(barPos, ImVec2(barPos.x + barWidth, barPos.y + barHeight), IM_COL32(40, 40, 40, 255), 4.0f);
                    drawList->AddLine(ImVec2(barPos.x + barWidth * 0.5f, barPos.y), ImVec2(barPos.x + barWidth * 0.5f, barPos.y + barHeight), IM_COL32(150, 150, 150, 255), 2.0f);
                    
                    float maxDelta = 2.0f; // Fondo scala a +- 2 secondi
                    float clampedDelta = std::clamp(currentDelta, -maxDelta, maxDelta);
                    float barFillWidth = (std::abs(clampedDelta) / maxDelta) * (barWidth * 0.5f);
                    
                    if (clampedDelta <= 0.0f) {
                        drawList->AddRectFilled(ImVec2(barPos.x + barWidth * 0.5f - barFillWidth, barPos.y), 
                                                ImVec2(barPos.x + barWidth * 0.5f, barPos.y + barHeight), 
                                                IM_COL32(50, 255, 50, 255), 4.0f);
                    } else { 
                        drawList->AddRectFilled(ImVec2(barPos.x + barWidth * 0.5f, barPos.y), 
                                                ImVec2(barPos.x + barWidth * 0.5f + barFillWidth, barPos.y + barHeight), 
                                                IM_COL32(255, 20, 20, 255), 4.0f);
                    }
                    
                    ImGui::Dummy(ImVec2(col2Width, ImGui::GetContentRegionAvail().y)); 
                } else {
                    DrawAdaptiveTextCol2("--.--", 0.95f, 0.25f, IM_COL32(100, 100, 100, 255)); 
                    ImGui::Dummy(ImVec2(col2Width, ImGui::GetContentRegionAvail().y)); 
                }

                // ====================================================================
                // COLONNA 3: MAPPA DELLA PISTA IN TEMPO REALE
                // ====================================================================
                ImGui::TableSetColumnIndex(2);

                float col3Width = ImGui::GetContentRegionAvail().x;

                // Allinea verticalmente il top
                ImGui::Dummy(ImVec2(0, col1Height * 0.05f)); 

                // Controlli Zoom
                ImGui::BeginGroup();
                float btnW = 80.0f * scale;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (col3Width - (btnW * 2 + 10.0f * scale)) * 0.5f);
                if (ImGui::Button("ZOOM -", ImVec2(btnW, 35.0f * scale))) currentLiveZoom -= 0.5f;
                ImGui::SameLine();
                if (ImGui::Button("ZOOM +", ImVec2(btnW, 35.0f * scale))) currentLiveZoom += 0.5f;
                ImGui::EndGroup();

                ImGui::Dummy(ImVec2(0, 10.0f * scale));

                // Area della Mappa
                ImVec2 mapCanvasPos = ImGui::GetCursorScreenPos();
                ImVec2 mapCanvasSize = ImGui::GetContentRegionAvail(); // Riempie tutto il restante spazio

                if (currentLiveZoom > 20.0f) currentLiveZoom = 20.0f;
                if (currentLiveZoom < 10.0f) currentLiveZoom = 10.0f;

                // Mappa centrata sul kart in tempo reale usando le coordinate fluide
                mapView.CenterOn(smoothLat, smoothLon, currentLiveZoom); 

                bool isMapHovered, isMapActive;
                mapView.RenderBaseMap(renderer, mapCanvasPos, mapCanvasSize, false, isMapHovered, isMapActive);

                drawList->PushClipRect(mapCanvasPos, ImVec2(mapCanvasPos.x + mapCanvasSize.x, mapCanvasPos.y + mapCanvasSize.y), true);
                ImVec2 canvasCenter(mapCanvasPos.x + mapCanvasSize.x * 0.5f, mapCanvasPos.y + mapCanvasSize.y * 0.5f);

                // Disegno Fences (Traguardo e Settori)
                for (const auto& fence : selectedTrack.fences) {
                    ImVec2 p1 = mapView.GetScreenPos(fence.p1.lat, fence.p1.lon, canvasCenter);
                    ImVec2 p2 = mapView.GetScreenPos(fence.p2.lat, fence.p2.lon, canvasCenter);
                    ImU32 fColor = (fence.type == TrackFenceType::FinishLine) ? IM_COL32(50, 255, 50, 255) : IM_COL32(255, 150, 0, 255);
                    drawList->AddLine(p1, p2, fColor, 4.0f);
                }

                // Disegno Traiettorie (con decimazione anti-lag)
                auto drawTrail = [&](const std::vector<GNSSFrame>& traj, ImU32 color, float thickness) {
                    if (traj.empty()) return;
                    ImVec2 lastPt(-9999, -9999);
                    size_t step = (traj.size() > 150) ? (traj.size() / 150) : 1;
                    for (size_t i = 0; i < traj.size(); i += step) {
                        ImVec2 sp = mapView.GetScreenPos(traj[i].lat, traj[i].lon, canvasCenter);
                        if (lastPt.x != -9999) drawList->AddLine(lastPt, sp, color, thickness);
                        lastPt = sp;
                    }
                };

                // Traccia Best (Fantasma), Traccia Precedente, Traccia Corrente
                drawTrail(bestTraj, IM_COL32(200, 50, 255, 180), 8.0f); 
                drawTrail(processor.GetPreviousTrajectory(), IM_COL32(150, 150, 150, 150), 4.0f); 
                drawTrail(processor.GetCurrentTrajectory(), IM_COL32(255, 255, 255, 255), 4.0f); 

                // Disegno Kart Direzionale in tempo reale
                ImVec2 kartPos = mapView.GetScreenPos(smoothLat, smoothLon, canvasCenter);
                float angle = (smoothHead - 90.0f) * M_PI / 180.0f;
                float kartSize = 25.0f * scale; 
                
                ImVec2 p1(kartPos.x + std::cos(angle) * kartSize, kartPos.y + std::sin(angle) * kartSize);
                ImVec2 p2(kartPos.x + std::cos(angle + 2.5f) * kartSize, kartPos.y + std::sin(angle + 2.5f) * kartSize);
                ImVec2 p3(kartPos.x + std::cos(angle - 2.5f) * kartSize, kartPos.y + std::sin(angle - 2.5f) * kartSize);
                
                drawList->AddTriangleFilled(p1, p2, p3, IM_COL32(255, 50, 50, 255));
                drawList->AddTriangle(p1, p2, p3, IM_COL32(255, 255, 255, 255), 2.0f);

                drawList->PopClipRect();

                ImGui::PopStyleVar(); 
            }
            ImGui::EndTable();
        }
    }
    
    ImGui::EndChild();
    ImGui::PopStyleVar();
}