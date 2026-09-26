#include "LapComparisonLayout.hpp"
#include "../views/DataWorksheetView.hpp"
#include "../components/TelemetryImportModal.hpp"
#include "../../utils/GeoMath.hpp"
#include <imgui.h>
#include <implot.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

LapComparisonLayout::LapComparisonLayout() {}

void LapComparisonLayout::ExtractSelectedLapsData(const Session& session) {
    std::string currentHash = "";
    for (const auto& d : session.drivers) {
        std::string selKey = std::string(session.sessionName) + "::" + d.name;
        for (const auto& kv : GlobalLapSelection[selKey]) {
            if (kv.second) {
                currentHash += d.name + std::to_string(kv.first);
                
                // TRUCCO: Includiamo un dato matematico della telemetria nel nome dell'hash.
                // Così se premi "Recalculate", il dato filtrato cambia e il grafico si ridisegna da solo all'istante!
                if (GlobalTelemetryCache.find(d.telemetryPath) != GlobalTelemetryCache.end()) {
                    const auto& cache = GlobalTelemetryCache[d.telemetryPath];
                    if (cache.frames.size() > 100) {
                        currentHash += "_" + std::to_string(cache.frames[cache.frames.size() / 2].accX);
                    }
                }
            }
        }
    }

    if (currentHash == lastSelectionHash) return;
    lastSelectionHash = currentHash;

    extractedData.clear();
    maxPlaybackDist = 0.0;
    int colorIdx = 0;

    for (const auto& d : session.drivers) {
        if (d.telemetryPath.empty() || GlobalTelemetryCache.find(d.telemetryPath) == GlobalTelemetryCache.end()) continue;
        
        std::string selKey = std::string(session.sessionName) + "::" + d.name;
        const auto& cache = GlobalTelemetryCache[d.telemetryPath];

        for (const auto& lap : cache.laps) {
            if (GlobalLapSelection[selKey][lap.lapNumber]) {
                LapPlotData plot;
                plot.legendName = d.name + " - L" + std::to_string(lap.lapNumber);
                
                ImVec4 implotColor = ImPlot::GetColormapColor(colorIdx);
                plot.color = ImGui::ColorConvertFloat4ToU32(implotColor);
                colorIdx++;
                
                double drawStartUs = lap.startTime * 1000000.0;
                double drawEndUs = lap.endTime * 1000000.0;
                
                double currentDist = 0.0;
                double lastTime = -1.0;

                for (const auto& frame : cache.frames) {
                    if (frame.time >= drawStartUs && frame.time <= drawEndUs) {
                        
                        double speedMs = frame.speed / 1000.0; 
                        if (std::isnan(speedMs) || std::isinf(speedMs)) speedMs = 0.0;

                        double dt = 0.0;
                        if (lastTime > 0.0) {
                            dt = (frame.time - lastTime) / 1000000.0;
                            currentDist += speedMs * dt; 
                        }
                        
                        plot.distance.push_back(currentDist);
                        plot.timeElapsed.push_back((frame.time - drawStartUs) / 1000000.0);
                        plot.speed.push_back(speedMs * 3.6); 
                        plot.lats.push_back(frame.lat);
                        plot.lons.push_back(frame.lon);
                        
                        double safeAx = frame.accX;
                        double safeAy = frame.accY;
                        if (std::isnan(safeAx) || std::isinf(safeAx)) safeAx = 0.0;
                        if (std::isnan(safeAy) || std::isinf(safeAy)) safeAy = 0.0;
                        
                        plot.latAcc.push_back(safeAx);
                        plot.lonAcc.push_back(safeAy);
                        
                        lastTime = frame.time;
                    }
                }
                if (!plot.distance.empty()) {
                    if (currentDist > maxPlaybackDist) maxPlaybackDist = currentDist;
                    extractedData.push_back(plot);
                }
            }
        }
    }

    if (!extractedData.empty()) {
        size_t bestLapIdx = 0;
        double bestTime = extractedData[0].timeElapsed.back();
        for (size_t i = 1; i < extractedData.size(); ++i) {
            if (extractedData[i].timeElapsed.back() < bestTime) {
                bestTime = extractedData[i].timeElapsed.back();
                bestLapIdx = i;
            }
        }

        const auto& refLap = extractedData[bestLapIdx];

        for (auto& plot : extractedData) {
            plot.delta.reserve(plot.distance.size());
            for (size_t i = 0; i < plot.distance.size(); ++i) {
                double curDist = plot.distance[i];
                double curTime = plot.timeElapsed[i];
                double refTime = 0.0;
                
                if (curDist >= refLap.distance.back()) {
                    double extraDist = curDist - refLap.distance.back();
                    double lastSpeed = refLap.speed.back() / 3.6; 
                    if (lastSpeed < 1.0) lastSpeed = 1.0; 
                    refTime = refLap.timeElapsed.back() + (extraDist / lastSpeed);
                } 
                else if (curDist <= refLap.distance.front()) {
                    refTime = refLap.timeElapsed.front();
                } 
                else {
                    auto it = std::lower_bound(refLap.distance.begin(), refLap.distance.end(), curDist);
                    size_t idx = std::distance(refLap.distance.begin(), it);
                    double d1 = refLap.distance[idx - 1];
                    double d2 = refLap.distance[idx];
                    double t1 = refLap.timeElapsed[idx - 1];
                    double t2 = refLap.timeElapsed[idx];
                    
                    double factor = (d2 == d1) ? 0.0 : (curDist - d1) / (d2 - d1);
                    refTime = t1 + factor * (t2 - t1);
                }

                double currentDelta = curTime - refTime;
                if (std::isnan(currentDelta) || std::isinf(currentDelta)) currentDelta = 0.0;
                
                plot.delta.push_back(currentDelta);
            }
        }
    }

    needsMapCentering = true;
}

void LapComparisonLayout::Render(const Session& session, SDL_Renderer* renderer) {
    ExtractSelectedLapsData(session);

    ImGuiIO& io = ImGui::GetIO();
    float scale = io.FontGlobalScale;
    
    if (isPlaying && !extractedData.empty()) {
        const auto& refPlot = extractedData[0]; 
        auto it = std::lower_bound(refPlot.distance.begin(), refPlot.distance.end(), currentPlaybackDist);
        double currentRealSpeedMs = 1.0; 
        
        if (it != refPlot.distance.end() && it != refPlot.distance.begin()) {
            size_t idx = std::distance(refPlot.distance.begin(), it);
            currentRealSpeedMs = refPlot.speed[idx] / 3.6; 
        }
        
        currentPlaybackDist += (currentRealSpeedMs * playbackSpeedMultiplier) * io.DeltaTime;
        if (currentPlaybackDist >= maxPlaybackDist) {
            currentPlaybackDist = maxPlaybackDist;
            isPlaying = false;
        }
    }

    float timelineHeight = 50.0f * scale;
    float contentHeight = ImGui::GetContentRegionAvail().y - timelineHeight;

    if (ImGui::BeginChild("UpperContent", ImVec2(0, contentHeight), false)) {
        
        if (ImGui::BeginTable("LapCompTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Visuals", ImGuiTableColumnFlags_WidthStretch, 0.40f);
            ImGui::TableSetupColumn("Graphs", ImGuiTableColumnFlags_WidthStretch, 0.60f);
            ImGui::TableNextRow();

            // --- COLONNA SINISTRA (Mappa e Video) ---
            ImGui::TableSetColumnIndex(0);
            ImVec2 leftAvail = ImGui::GetContentRegionAvail();
            
            if (ImGui::BeginChild("VideoRegion", ImVec2(0, leftAvail.y * 0.4f), true)) {
                ImVec2 center = ImVec2(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x * 0.5f - 50, 
                                       ImGui::GetCursorPos().y + ImGui::GetContentRegionAvail().y * 0.5f - 10);
                ImGui::SetCursorPos(center);
                ImGui::TextDisabled("VIDEO PLAYER");
            }
            ImGui::EndChild();

            if (ImGui::BeginChild("MapRegion", ImVec2(0, 0), true)) {
                ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                ImVec2 canvasSize = ImGui::GetContentRegionAvail();
                
                if (needsMapCentering && !session.selectedTrack.fences.empty()) {
                    double cLat, cLon; float cZoom;
                    GeoMath::CalculateTrackCenterAndZoom(session.selectedTrack, canvasSize, 0.15f, cLat, cLon, cZoom);
                    miniMap.CenterOn(cLat, cLon, cZoom);
                    needsMapCentering = false;
                }

                bool hov, act;
                miniMap.RenderBaseMap(renderer, canvasPos, canvasSize, true, hov, act);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);
                ImVec2 canvasCenter(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);
                
                for (const auto& f : session.selectedTrack.fences) {
                    ImVec2 sp1 = miniMap.GetScreenPos(f.p1.lat, f.p1.lon, canvasCenter);
                    ImVec2 sp2 = miniMap.GetScreenPos(f.p2.lat, f.p2.lon, canvasCenter);
                    drawList->AddLine(sp1, sp2, IM_COL32(255, 255, 255, 100), 2.0f);
                }

                double referenceTime = 0.0;
                if (!extractedData.empty()) {
                    auto refIt = std::lower_bound(extractedData[0].distance.begin(), extractedData[0].distance.end(), currentPlaybackDist);
                    if (refIt != extractedData[0].distance.end()) {
                        size_t refIdx = std::distance(extractedData[0].distance.begin(), refIt);
                        referenceTime = extractedData[0].timeElapsed[refIdx];
                    }
                }

                for (const auto& plot : extractedData) {
                    ImVec2 lastPt(-9999, -9999);
                    for (size_t i = 0; i < plot.lats.size(); ++i) {
                        ImVec2 sp = miniMap.GetScreenPos(plot.lats[i], plot.lons[i], canvasCenter);
                        if (lastPt.x != -9999 && std::hypot(sp.x - lastPt.x, sp.y - lastPt.y) > 1.0f) {
                            ImU32 dimColor = (plot.color & 0x00FFFFFF) | 0xD0000000;
                            drawList->AddLine(lastPt, sp, dimColor, 4.0f);
                            lastPt = sp;
                        } else if (lastPt.x == -9999) {
                            lastPt = sp;
                        }
                    }

                    auto it = std::lower_bound(plot.timeElapsed.begin(), plot.timeElapsed.end(), referenceTime);
                    if (it != plot.timeElapsed.end() && it != plot.timeElapsed.begin()) {
                        size_t idx = std::distance(plot.timeElapsed.begin(), it);
                        ImVec2 pRef = miniMap.GetScreenPos(plot.lats[idx], plot.lons[idx], canvasCenter);
                        ImVec2 pPrev = miniMap.GetScreenPos(plot.lats[idx-1], plot.lons[idx-1], canvasCenter);
                        ImVec2 pOffset = miniMap.GetScreenPos(plot.lats[idx] + 0.0001, plot.lons[idx], canvasCenter);
                        float pxPerMeter = std::abs(pRef.y - pOffset.y) / 11.132f;
                        if (pxPerMeter < 0.1f) pxPerMeter = 0.1f;
                        
                        float w = std::max(1.4f * pxPerMeter * 0.5f, 2.0f); 
                        float l = std::max(2.1f * pxPerMeter * 0.5f, 3.0f); 
                        float angle = std::atan2(pRef.y - pPrev.y, pRef.x - pPrev.x);
                        
                        ImVec2 pA(pRef.x + std::cos(angle)*l - std::sin(angle)*w, pRef.y + std::sin(angle)*l + std::cos(angle)*w);
                        ImVec2 pB(pRef.x + std::cos(angle)*l + std::sin(angle)*w, pRef.y + std::sin(angle)*l - std::cos(angle)*w);
                        ImVec2 pC(pRef.x - std::cos(angle)*l + std::sin(angle)*w, pRef.y - std::sin(angle)*l - std::cos(angle)*w);
                        ImVec2 pD(pRef.x - std::cos(angle)*l - std::sin(angle)*w, pRef.y - std::sin(angle)*l + std::cos(angle)*w);
                        
                        drawList->AddQuadFilled(pA, pB, pC, pD, plot.color);
                        drawList->AddQuad(pA, pB, pC, pD, IM_COL32(0,0,0,255), 1.0f);
                    }
                }
                
                drawList->PopClipRect();
                miniMap.RenderUIControls(canvasPos, canvasSize);
            }
            ImGui::EndChild();

            // --- COLONNA DESTRA (Grafici Interattivi Nativi) ---
            ImGui::TableSetColumnIndex(1);
            
            if (ImPlot::BeginSubplots("Telemetria", 4, 1, ImVec2(-1, -1), ImPlotSubplotFlags_LinkAllX | ImPlotSubplotFlags_NoTitle)) {
                
                // GRAFICO 1: DELTA
                if (ImPlot::BeginPlot("Time Delta (s)")) {
                    ImPlot::SetupAxes("##X", "Delta (s)", ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupAxisLimits(ImAxis_X1, 0, maxPlaybackDist, ImPlotCond_Once);
                    
                    for (const auto& plot : extractedData) {
                        std::string uniqueID = plot.legendName + "##Delta";
                        ImPlot::PlotLine(uniqueID.c_str(), plot.distance.data(), plot.delta.data(), plot.distance.size());
                    }
                    
                    if (ImPlot::DragLineX(1001, &currentPlaybackDist, ImVec4(1, 1, 1, 0.8f), 1.5f)) isPlaying = false;
                    
                    ImPlot::EndPlot();
                }

                // GRAFICO 2: SPEED
                if (ImPlot::BeginPlot("Speed (km/h)")) {
                    ImPlot::SetupAxes("##X", "Speed", ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupAxisLimits(ImAxis_X1, 0, maxPlaybackDist, ImPlotCond_Once);
                    
                    for (const auto& plot : extractedData) {
                        std::string uniqueID = plot.legendName + "##Speed";
                        ImPlot::PlotLine(uniqueID.c_str(), plot.distance.data(), plot.speed.data(), plot.distance.size());
                    }
                    
                    if (ImPlot::DragLineX(1002, &currentPlaybackDist, ImVec4(1, 1, 1, 0.8f), 1.5f)) isPlaying = false;
                    
                    ImPlot::EndPlot();
                }

                // GRAFICO 3: LAT ACCEL
                if (ImPlot::BeginPlot("Lat Accel (G)")) {
                    ImPlot::SetupAxes("##X", "G", ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupAxisLimits(ImAxis_X1, 0, maxPlaybackDist, ImPlotCond_Once);
                    
                    for (const auto& plot : extractedData) {
                        std::string uniqueID = plot.legendName + "##Lat";
                        ImPlot::PlotLine(uniqueID.c_str(), plot.distance.data(), plot.latAcc.data(), plot.distance.size());
                    }
                    
                    if (ImPlot::DragLineX(1003, &currentPlaybackDist, ImVec4(1, 1, 1, 0.8f), 1.5f)) isPlaying = false;
                    
                    ImPlot::EndPlot();
                }

                // GRAFICO 4: LON ACCEL (Questo mostra i numeri dell'Asse X)
                if (ImPlot::BeginPlot("Lon Accel (G)")) {
                    ImPlot::SetupAxes("Distance (m)", "G", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupAxisLimits(ImAxis_X1, 0, maxPlaybackDist, ImPlotCond_Once);
                    
                    for (const auto& plot : extractedData) {
                        std::string uniqueID = plot.legendName + "##Lon";
                        ImPlot::PlotLine(uniqueID.c_str(), plot.distance.data(), plot.lonAcc.data(), plot.distance.size());
                    }
                    
                    if (ImPlot::DragLineX(1004, &currentPlaybackDist, ImVec4(1, 1, 1, 0.8f), 1.5f)) isPlaying = false;
                    
                    ImPlot::EndPlot();
                }

                ImPlot::EndSubplots();
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    
    // --- TIMELINE CONTROLS INFERIORI ---
    if (ImGui::BeginChild("TimelineRegion", ImVec2(0, 0), false)) {
        ImGui::Dummy(ImVec2(0, 5.0f * scale));

        ImVec4 playColor = isPlaying ? ImVec4(0.8f, 0.4f, 0.2f, 1.0f) : ImVec4(0.2f, 0.6f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, playColor);
        if (ImGui::Button(isPlaying ? "|| PAUSE" : "> PLAY", ImVec2(100.0f * scale, 0))) {
            isPlaying = !isPlaying;
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f * scale);
        ImGui::SliderFloat("Speed", &playbackSpeedMultiplier, 0.1f, 3.0f, "%.1fx");

        ImGui::SameLine();
        
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10.0f);
        double zero = 0.0;
        if (ImGui::SliderScalar("##TimeScrubber", ImGuiDataType_Double, &currentPlaybackDist, &zero, &maxPlaybackDist, "Distance: %.1f m")) {
            isPlaying = false; 
        }
    }
    ImGui::EndChild();
}