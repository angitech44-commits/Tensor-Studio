#include "TelemetryImportModal.hpp"
#include "../../utils/FileDialog.hpp"
#include "../../io/FileParser.hpp"
#include "../../analysis/SpatialResampler.hpp"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cctype>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TelemetryImportModal::TelemetryImportModal() : dialogMutex(std::make_unique<std::mutex>()) {}

void TelemetryImportModal::Open(std::string* telemetryPathPtr, const Track& track) {
    targetTelemetryPath = telemetryPathPtr;
    tempFilePath = targetTelemetryPath ? *targetTelemetryPath : "";
    activeTrack = track;
    
    parsedLaps.clear();
    fusedTelemetry.clear();
    rawImu.clear();
    rawGnss.clear();
    
    parsedRowCount = 0;
    totalFileDuration = 0.0;
    sessionBaseTime = 0.0;
    shouldOpen = true;
    triggerImport = false;
    needsMapCentering = false;
    pendingPath.clear();

    if (!tempFilePath.empty()) {
        ParseTelemetryFile(tempFilePath);
    }
}

void TelemetryImportModal::ParseTelemetryFile(const std::string& path) {
    parsedLaps.clear();
    fusedTelemetry.clear();
    rawImu.clear();
    rawGnss.clear();
    
    parsedRowCount = 0;
    totalFileDuration = 0.0;
    
    if (path.empty()) return;

    std::string filename = path.substr(path.find_last_of("/\\") + 1);
    std::string baseName = filename.substr(0, filename.find_last_of("."));
    std::string digitsOnly;
    for (char c : baseName) {
        if (std::isdigit(c)) digitsOnly += c;
    }
    sessionBaseTime = 0.0;
    if (!digitsOnly.empty()) {
        try { sessionBaseTime = std::stod(digitsOnly); } catch (...) {}
    }

    FileParser parser;
    parser.ParseCSV(path);
    
    rawImu = parser.imuData;
    rawGnss = parser.gnssData;
    
    SpatialResampler resampler;
    fusedTelemetry = resampler.Resample(parser.imuData, parser.gnssData);
    
    parsedRowCount = fusedTelemetry.size();
    if (parsedRowCount > 0) {
        double firstValid = 0.0;
        double lastValid = 0.0;
        
        for (const auto& frame : fusedTelemetry) {
            if (std::abs(frame.lat) > 1.0 && std::abs(frame.lon) > 1.0) {
                if (firstValid == 0.0) firstValid = frame.time;
                lastValid = frame.time;
            }
        }
        
        if (firstValid > 0.0 && lastValid >= firstValid) {
            totalFileDuration = (lastValid - firstValid) / 1000000.0;
            needsMapCentering = true; 
        }
    }

    LapSplitter splitter;
    parsedLaps = splitter.SplitLaps(fusedTelemetry, activeTrack, sessionBaseTime);
}

std::string TelemetryImportModal::FormatDurationString(double totalSeconds) {
    if (totalSeconds < 0.0) return "0.0 sec";
    
    if (totalSeconds >= 3600.0) {
        int h = static_cast<int>(totalFileDuration) / 3600;
        int m = (static_cast<int>(totalFileDuration) % 3600) / 60;
        float s = totalFileDuration - (h * 3600) - (m * 60);
        char buf[64];
        snprintf(buf, sizeof(buf), "%dh %dm %.1fs", h, m, s);
        return std::string(buf);
    } else if (totalSeconds >= 60.0) {
        int m = static_cast<int>(totalFileDuration) / 60;
        float s = totalFileDuration - (m * 60);
        char buf[64];
        snprintf(buf, sizeof(buf), "%dm %.1fs", m, s);
        return std::string(buf);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f sec", totalSeconds);
        return std::string(buf);
    }
}

std::string TelemetryImportModal::FormatLapTime(float seconds) {
    if (seconds < 0 || std::isnan(seconds) || std::isinf(seconds)) return "0:00.000";
    int m = static_cast<int>(seconds) / 60;
    float s = seconds - (m * 60);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d:%06.3f", m, s);
    return std::string(buf);
}

std::string TelemetryImportModal::FormatDate(double unixTimeSec) {
    if (unixTimeSec < 1.0) return "N/A";
    
    int trackTimezoneOffsetSec = 0;
    if (!activeTrack.fences.empty()) {
        trackTimezoneOffsetSec = static_cast<int>(std::round(activeTrack.fences[0].p1.lon / 15.0)) * 3600;
    }
    
    std::time_t t = static_cast<std::time_t>(unixTimeSec) + trackTimezoneOffsetSec;
    std::tm* tm_info = std::gmtime(&t);
    
    char buf[32] = "N/A";
    if (tm_info) std::strftime(buf, sizeof(buf), "%Y-%m-%d", tm_info);
    return std::string(buf);
}

void TelemetryImportModal::Render(SDL_Renderer* renderer) {
    if (shouldOpen) {
        ImGui::OpenPopup("Import Telemetry Data");
        shouldOpen = false;
        isOpen = true;
    }

    if (!isOpen) return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 center = viewport->GetCenter();
    float scale = ImGui::GetIO().FontGlobalScale;
    
    float margin = 100.0f * scale; 
    ImVec2 modalSize = ImVec2(viewport->Size.x - margin, viewport->Size.y - margin);

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(modalSize, ImGuiCond_Always);

    bool keepOpen = true; 
    bool isModalRendering = ImGui::BeginPopupModal("Import Telemetry Data", &keepOpen, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    if (!keepOpen) {
        isOpen = false;
    }

    if (isModalRendering) {
        
        std::string loadedPath;
        {
            std::lock_guard<std::mutex> lock(*dialogMutex);
            if (triggerImport) {
                loadedPath = pendingPath;
                triggerImport = false;
                pendingPath.clear();
            }
        }
        
        if (!loadedPath.empty()) {
            tempFilePath = loadedPath;
            ParseTelemetryFile(tempFilePath);
        }

        ImGui::Dummy(ImVec2(0, 5.0f * scale));
        ImGui::Text("Data Source:");
        ImGui::SameLine();
        
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - (100.0f * scale));
        char pathBuf[256];
        strncpy(pathBuf, tempFilePath.c_str(), sizeof(pathBuf));
        ImGui::InputText("##filepath", pathBuf, sizeof(pathBuf), ImGuiInputTextFlags_ReadOnly);
        
        ImGui::SameLine();
        if (ImGui::Button("Browse...", ImVec2(90.0f * scale, 0))) {
            std::vector<SDL_DialogFileFilter> filters = {
                { "All Files", "*" },
                { "CSV Files", "csv" },
                { "Text/Log Files", "txt;log" }
            };

            FileDialog::OpenFile(nullptr, [this](const std::string& path) {
                if (!path.empty()) {
                    std::lock_guard<std::mutex> lock(*dialogMutex);
                    pendingPath = path;
                    triggerImport = true;
                }
            }, filters);
        }

        if (parsedRowCount > 0 && !fusedTelemetry.empty()) {
            std::string recDate = (sessionBaseTime > 0.0) ? FormatDate(sessionBaseTime) : "N/A";
            std::string durStr = FormatDurationString(totalFileDuration);
            
            char tzBuf[16] = "UTC";
            if (!activeTrack.fences.empty()) {
                int tzHours = static_cast<int>(std::round(activeTrack.fences[0].p1.lon / 15.0));
                snprintf(tzBuf, sizeof(tzBuf), "UTC%s%d", (tzHours >= 0 ? "+" : ""), tzHours);
            }
            
            ImGui::Dummy(ImVec2(0, 2.0f * scale));
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Date: %s (%s) | Samples: %d | Duration: %s", recDate.c_str(), tzBuf, parsedRowCount, durStr.c_str());
        }

        ImGui::Dummy(ImVec2(0, 10.0f * scale));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5.0f * scale));

        ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "SESSION PREVIEW & TRAJECTORY");
        ImGui::Dummy(ImVec2(0, 5.0f * scale));
        
        if (parsedLaps.empty() && fusedTelemetry.empty()) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            const char* msg = "Select a Tensor telemetry file to parse laps and preview data.";
            ImVec2 textSize = ImGui::CalcTextSize(msg);
            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (avail.x - textSize.x) * 0.5f, ImGui::GetCursorPosY() + 40.0f * scale));
            ImGui::TextDisabled("%s", msg);
        } else {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float contentHeight = avail.y - 45.0f * scale; 
            float tableWidth = avail.x * 0.45f;
            
            ImGui::BeginChild("TablePane", ImVec2(tableWidth, contentHeight), false);
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10.0f * scale, 8.0f * scale));

            if (ImGui::BeginTable("LapsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                
                ImGui::TableSetupColumn("Lap", ImGuiTableColumnFlags_WidthFixed, 45.0f * scale);
                ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80.0f * scale);
                ImGui::TableSetupColumn("Start", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("End", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                for (const auto& lap : parsedLaps) {
                    ImGui::TableNextRow();
                    
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text(" %d", lap.lapNumber);
                    
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "%s", FormatLapTime(lap.timeSeconds).c_str());
                    
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%s", lap.startLocalTime.c_str());
                    
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%s", lap.endLocalTime.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleVar(); 
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("MapPane", ImVec2(0, contentHeight), true);
            
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 canvasCenter(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);

            if (needsMapCentering && !fusedTelemetry.empty() && canvasSize.x > 0 && canvasSize.y > 0) {
                double minLat = 90.0, maxLat = -90.0, minLon = 180.0, maxLon = -180.0;
                bool validBounds = false;
                for (const auto& frame : fusedTelemetry) {
                    if (std::abs(frame.lat) > 1.0 && std::abs(frame.lon) > 1.0) {
                        minLat = std::min(minLat, frame.lat);
                        maxLat = std::max(maxLat, frame.lat);
                        minLon = std::min(minLon, frame.lon);
                        maxLon = std::max(maxLon, frame.lon);
                        validBounds = true;
                    }
                }
                if (validBounds) {
                    double cLat = (minLat + maxLat) / 2.0;
                    double cLon = (minLon + maxLon) / 2.0;
                    
                    auto latToY = [](double lat) {
                        double latRad = lat * M_PI / 180.0;
                        return (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0;
                    };
                    auto lonToX = [](double lon) { return (lon + 180.0) / 360.0; };
                    
                    double dx = std::abs(lonToX(maxLon) - lonToX(minLon));
                    double dy = std::abs(latToY(maxLat) - latToY(minLat));
                    
                    if (dx == 0) dx = 0.00001;
                    if (dy == 0) dy = 0.00001;
                    
                    double targetWidth = canvasSize.x * 0.85; 
                    double targetHeight = canvasSize.y * 0.85;
                    
                    double zoomX = std::log2(targetWidth / (dx * 256.0));
                    double zoomY = std::log2(targetHeight / (dy * 256.0));
                    
                    float dynamicZoom = (float)std::min(zoomX, zoomY);
                    dynamicZoom = std::max(2.0f, std::min(21.0f, dynamicZoom));
                    
                    mapView.CenterOn(cLat, cLon, dynamicZoom); 
                }
                needsMapCentering = false;
            }

            bool isMapHovered = false;
            bool isMapActive = false;
            
            mapView.RenderBaseMap(renderer, canvasPos, canvasSize, true, isMapHovered, isMapActive);

            drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);
            
            float distThreshold = 1.5f;
            if (fusedTelemetry.size() > 10000) {
                distThreshold = 1.5f + (float)(fusedTelemetry.size() / 10000) * 0.5f;
            }

            ImVec2 lastPt(-9999.0f, -9999.0f);
            int drawnSegments = 0;
            
            for (const auto& frame : fusedTelemetry) {
                if (std::abs(frame.lat) > 1.0 && std::abs(frame.lon) > 1.0) {
                    ImVec2 sp = mapView.GetScreenPos(frame.lat, frame.lon, canvasCenter);
                    
                    if (lastPt.x == -9999.0f) {
                        lastPt = sp;
                    } else {
                        if (std::hypot(sp.x - lastPt.x, sp.y - lastPt.y) > distThreshold) {
                            float minX = std::min(lastPt.x, sp.x);
                            float maxX = std::max(lastPt.x, sp.x);
                            float minY = std::min(lastPt.y, sp.y);
                            float maxY = std::max(lastPt.y, sp.y);
                            
                            bool inView = !(maxX < canvasPos.x || minX > canvasPos.x + canvasSize.x || 
                                            maxY < canvasPos.y || minY > canvasPos.y + canvasSize.y);
                                            
                            if (inView) {
                                drawList->AddLine(lastPt, sp, IM_COL32(0, 255, 255, 200), 2.0f);
                                drawnSegments++;
                                if (drawnSegments > 14000) break; 
                            }
                            lastPt = sp;
                        }
                    }
                }
            }

            for (const auto& fence : activeTrack.fences) {
                ImVec2 sp1 = mapView.GetScreenPos(fence.p1.lat, fence.p1.lon, canvasCenter);
                ImVec2 sp2 = mapView.GetScreenPos(fence.p2.lat, fence.p2.lon, canvasCenter);
                ImU32 fenceColor = (fence.type == TrackFenceType::FinishLine) ? IM_COL32(50, 255, 50, 255) : IM_COL32(255, 150, 0, 255);
                drawList->AddLine(sp1, sp2, fenceColor, 3.0f);
            }

            drawList->PopClipRect();
            mapView.RenderUIControls(canvasPos, canvasSize);

            ImGui::EndChild();
        }

        ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 40.0f * scale);
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5.0f * scale));
        
        ImGui::SetCursorPosX(ImGui::GetWindowSize().x - 225.0f * scale);
        
        if (ImGui::Button("Cancel", ImVec2(100.0f * scale, 0))) {
            ImGui::CloseCurrentPopup();
            isOpen = false;
        }
        ImGui::SameLine();
        
        ImGui::BeginDisabled(tempFilePath.empty());
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        if (ImGui::Button("Confirm", ImVec2(100.0f * scale, 0))) {
            if (targetTelemetryPath) *targetTelemetryPath = tempFilePath;
            
            // ORA SALVIAMO TUTTO NELLA CACHE: Fuso, Giri, BaseTime, ImuRaw, GnssRaw
            GlobalTelemetryCache[tempFilePath] = { fusedTelemetry, parsedLaps, sessionBaseTime, rawImu, rawGnss };
            
            ImGui::CloseCurrentPopup();
            isOpen = false;
        }
        ImGui::PopStyleColor(2);
        ImGui::EndDisabled();
        
        if (!isOpen) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}