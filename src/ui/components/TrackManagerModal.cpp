#include "TrackManagerModal.hpp"
#include "../../io/TrackSerializer.hpp"
#include "../../utils/FileDialog.hpp"
#include "../../utils/GeoMath.hpp"
#include <imgui.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <cctype>

#ifndef PROJECT_ROOT_DIR
#define PROJECT_ROOT_DIR "."
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TrackManagerModal::TrackManagerModal() {}

void TrackManagerModal::RefreshTrackList() {
    savedTracks = TrackSerializer::GetAvailableTracks();
}


std::string TrackManagerModal::GetUniqueTrackName(const std::string& baseName) {
    std::string newName = baseName;
    int counter = 1;
    while (std::find(savedTracks.begin(), savedTracks.end(), newName) != savedTracks.end()) {
        newName = baseName + "_" + std::to_string(counter);
        counter++;
    }
    return newName;
}
// -----------------------------

void TrackManagerModal::Open() {
    shouldOpen = true; 
    isTrackActive = false;
    currentFileName = "";
    isPlacingFence = false;
    searchBuffer[0] = '\0';
    RefreshTrackList();
}

void TrackManagerModal::Render(SDL_Renderer* renderer) {
    if (shouldOpen) {
        ImGui::OpenPopup("Track Manager");
        shouldOpen = false;
        isOpen = true;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 center = viewport->GetCenter();
    
    // Recuperiamo la scala per i DPI
    float scale = ImGui::GetIO().FontGlobalScale;
    float margin = 100.0f * scale; // 50px per lato scalati
    
    // Calcoliamo la dimensione sottraendo il margine scalato
    ImVec2 modalSize = ImVec2(viewport->Size.x - margin, viewport->Size.y - margin);

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(modalSize, ImGuiCond_Always);

    bool keepOpen = true; 

    // Separiamo la lettura dello stato dal blocco if per evitare che il codice venga saltato alla chiusura
    bool isModalRendering = ImGui::BeginPopupModal("Track Manager", &keepOpen, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        
    // Intercettiamo il click sulla X (keepOpen diventa false) prima e a prescindere dal rendering
    if (!keepOpen) {
        if (isTrackActive && isTrackModified) {
            // Riapriamo forzatamente il Track Manager e facciamo scattare il popup
            ImGui::OpenPopup("Track Manager"); 
            showUnsavedChangesPopup = true;
            pendingTrackLoad = "##CLOSE_MANAGER##";
        } else {
            // Pista pulita: chiudiamo la nostra variabile di stato
            isOpen = false;
        }
    }

    // Se il popup è aperto, disegniamo il contenuto
    if (isModalRendering) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        
        float leftColWidth = 220.0f * scale;
        float rightColWidth = 260.0f * scale;
        float canvasWidth = avail.x - leftColWidth - rightColWidth - (16.0f * scale);

        if (ImGui::BeginChild("TrackLibrary", ImVec2(leftColWidth, 0), true)) RenderLibraryColumn();
        ImGui::EndChild();
        ImGui::SameLine();

        if (isTrackActive) {
            if (ImGui::BeginChild("MapCanvas", ImVec2(canvasWidth, 0), true)) RenderCanvasColumn(renderer);
            ImGui::EndChild();
            ImGui::SameLine();
            if (ImGui::BeginChild("EditorTools", ImVec2(rightColWidth, 0), true)) RenderToolsColumn();
            ImGui::EndChild();
        } else {
            if (ImGui::BeginChild("EmptyState", ImVec2(0, 0), true)) {
                ImVec2 emptyAvail = ImGui::GetContentRegionAvail();
                const char* msg = "Select a track or create a new one";
                ImVec2 textSize = ImGui::CalcTextSize(msg);
                ImGui::SetCursorPos(ImVec2((emptyAvail.x - textSize.x) * 0.5f, (emptyAvail.y - textSize.y) * 0.5f));
                ImGui::TextDisabled("%s", msg);
            }
            ImGui::EndChild();
        }
        if (!isOpen) {
            ImGui::CloseCurrentPopup(); // Chiude forzatamente il Track Manager!
        }

        ImGui::EndPopup();
    }
}

void TrackManagerModal::RenderCanvasColumn(SDL_Renderer* renderer) {
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasCenter(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);

    if (needsMapCentering) {
        double cLat, cLon;
        float cZoom;
        GeoMath::CalculateTrackCenterAndZoom(activeTrack, canvasSize, 0.15f, cLat, cLon, cZoom);
        mapView.CenterOn(cLat, cLon, cZoom);
        needsMapCentering = false;
    }

    bool isMapHovered = false;
    bool isMapActive = false;

    mapView.RenderBaseMap(renderer, canvasPos, canvasSize, (currentMode == TrackEditorMode::ViewOnly), isMapHovered, isMapActive);
    HandleMapInput(canvasCenter, isMapHovered);

    drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);
    RenderTrackOverlays(drawList, canvasCenter, isMapHovered);        
    drawList->PopClipRect();

    mapView.RenderUIControls(canvasPos, canvasSize); 
}

void TrackManagerModal::HandleMapInput(ImVec2 canvasCenter, bool isMapHovered) {
    ImVec2 mousePos = ImGui::GetMousePos();

    double fullScale = 256.0 * std::pow(2.0, mapView.mapZoom);
    double latRad = mapView.mapCenterLat * M_PI / 180.0;
    double cX_global = ((mapView.mapCenterLon + 180.0) / 360.0) * fullScale;
    double cY_global = ((1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0) * fullScale;

    double tX = cX_global + (mousePos.x - canvasCenter.x);
    double tY = cY_global + (mousePos.y - canvasCenter.y);
    double clickLon = (tX / fullScale) * 360.0 - 180.0;
    double nY = 1.0 - 2.0 * (tY / fullScale);
    double clickLat = std::atan(std::sinh(nY * M_PI)) * 180.0 / M_PI;
    
    float snapDist = 15.0f;

    if (isMapHovered || draggedPointType != DraggedPointType::None) {
        
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && draggedPointType == DraggedPointType::None) PerformUndo();

        bool canCloseLeft = false, canCloseRight = false;
        
        // Helper unificato per calcolare lo snap di chiusura linea
        auto checkCloseSnap = [&](const auto& bound, TrackEditorMode mode, bool& canClose) {
            if (currentMode == mode && bound.size() > 2) {
                ImVec2 firstPt = mapView.GetScreenPos(bound[0].lat, bound[0].lon, canvasCenter);
                ImVec2 lastPt = mapView.GetScreenPos(bound.back().lat, bound.back().lon, canvasCenter);
                if (std::hypot(firstPt.x - lastPt.x, firstPt.y - lastPt.y) > 1.0f && std::hypot(mousePos.x - firstPt.x, mousePos.y - firstPt.y) < snapDist) {
                    canClose = true; clickLat = bound[0].lat; clickLon = bound[0].lon;
                }
            }
        };

        checkCloseSnap(activeTrack.limits.leftBound, TrackEditorMode::DrawLeftBound, canCloseLeft);
        checkCloseSnap(activeTrack.limits.rightBound, TrackEditorMode::DrawRightBound, canCloseRight);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && currentMode != TrackEditorMode::ViewOnly) {
            bool interacted = false;
            bool stateSaved = false;

            auto trySaveState = [&]() { if (!stateSaved) { SaveStateForUndo(); stateSaved = true; } };

            if (currentMode == TrackEditorMode::DeletePoint) {
                auto tryDeleteFromBound = [&](auto& bound) {
                    if (interacted) return;
                    for (size_t i = 0; i < bound.size(); ++i) {
                        ImVec2 pt = mapView.GetScreenPos(bound[i].lat, bound[i].lon, canvasCenter);
                        if (std::hypot(mousePos.x - pt.x, mousePos.y - pt.y) < snapDist) {
                            trySaveState();
                            if (GeoMath::IsBoundClosed(bound) && (i == 0 || i == bound.size() - 1)) {
                                bound.erase(bound.begin());
                                bound.pop_back();
                                if (!bound.empty()) bound.push_back(bound.front());
                            } else bound.erase(bound.begin() + i);
                            interacted = true; break;
                        }
                    }
                };
                tryDeleteFromBound(activeTrack.limits.leftBound);
                tryDeleteFromBound(activeTrack.limits.rightBound);
                
                if (!interacted) {
                    for (size_t i = 0; i < activeTrack.fences.size(); ++i) {
                        ImVec2 p1 = mapView.GetScreenPos(activeTrack.fences[i].p1.lat, activeTrack.fences[i].p1.lon, canvasCenter);
                        ImVec2 p2 = mapView.GetScreenPos(activeTrack.fences[i].p2.lat, activeTrack.fences[i].p2.lon, canvasCenter);
                        ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
                        if (std::hypot(mousePos.x - p1.x, mousePos.y - p1.y) < snapDist || std::hypot(mousePos.x - p2.x, mousePos.y - p2.y) < snapDist || std::hypot(mousePos.x - mid.x, mousePos.y - mid.y) < 20.0f) {
                            trySaveState(); activeTrack.fences.erase(activeTrack.fences.begin() + i); interacted = true; break;
                        }
                    }
                }
            } 
            else {
                if (canCloseLeft) { trySaveState(); activeTrack.limits.leftBound.push_back({clickLat, clickLon}); interacted = true; } 
                else if (canCloseRight) { trySaveState(); activeTrack.limits.rightBound.push_back({clickLat, clickLon}); interacted = true; }

                auto checkDragPoint = [&](auto& bound, DraggedPointType type) {
                    if (interacted) return;
                    for (int i = 0; i < bound.size(); ++i) {
                        ImVec2 pt = mapView.GetScreenPos(bound[i].lat, bound[i].lon, canvasCenter);
                        if (std::hypot(mousePos.x - pt.x, mousePos.y - pt.y) < snapDist) { trySaveState(); draggedPointType = type; draggedPointIndex = i; interacted = true; break; }
                    }
                };

                if (currentMode == TrackEditorMode::DrawLeftBound) checkDragPoint(activeTrack.limits.leftBound, DraggedPointType::LeftBound);
                if (currentMode == TrackEditorMode::DrawRightBound) checkDragPoint(activeTrack.limits.rightBound, DraggedPointType::RightBound);
                
                if (currentMode == TrackEditorMode::PlaceFinishLine || currentMode == TrackEditorMode::PlaceSector) {
                    TrackFenceType fType = (currentMode == TrackEditorMode::PlaceFinishLine) ? TrackFenceType::FinishLine : TrackFenceType::Sector;
                    for (int i = 0; i < activeTrack.fences.size(); ++i) {
                        if (activeTrack.fences[i].type != fType) continue;
                        ImVec2 p1 = mapView.GetScreenPos(activeTrack.fences[i].p1.lat, activeTrack.fences[i].p1.lon, canvasCenter);
                        if (std::hypot(mousePos.x - p1.x, mousePos.y - p1.y) < snapDist) { trySaveState(); draggedPointType = DraggedPointType::FenceP1; draggedPointIndex = i; interacted = true; break; }
                        ImVec2 p2 = mapView.GetScreenPos(activeTrack.fences[i].p2.lat, activeTrack.fences[i].p2.lon, canvasCenter);
                        if (std::hypot(mousePos.x - p2.x, mousePos.y - p2.y) < snapDist) { trySaveState(); draggedPointType = DraggedPointType::FenceP2; draggedPointIndex = i; interacted = true; break; }
                    }
                }

                auto checkSplitLine = [&](auto& bound, DraggedPointType type) {
                    if (interacted || bound.size() < 2) return;
                    for (int i = 0; i < bound.size() - 1; ++i) {
                        ImVec2 p1 = mapView.GetScreenPos(bound[i].lat, bound[i].lon, canvasCenter);
                        ImVec2 p2 = mapView.GetScreenPos(bound[i+1].lat, bound[i+1].lon, canvasCenter);
                        if (GeoMath::DistancePointToSegment(mousePos, p1, p2) < snapDist) {
                            trySaveState(); bound.insert(bound.begin() + i + 1, {clickLat, clickLon}); draggedPointType = type; draggedPointIndex = i + 1; interacted = true; break;
                        }
                    }
                };

                if (currentMode == TrackEditorMode::DrawLeftBound) checkSplitLine(activeTrack.limits.leftBound, DraggedPointType::LeftBound);
                if (currentMode == TrackEditorMode::DrawRightBound) checkSplitLine(activeTrack.limits.rightBound, DraggedPointType::RightBound);

                if (!interacted) {
                    if (currentMode == TrackEditorMode::DrawLeftBound) {
                        if (!GeoMath::IsBoundClosed(activeTrack.limits.leftBound)) { trySaveState(); activeTrack.limits.leftBound.push_back({clickLat, clickLon}); }
                    }
                    else if (currentMode == TrackEditorMode::DrawRightBound) {
                        if (!GeoMath::IsBoundClosed(activeTrack.limits.rightBound)) { trySaveState(); activeTrack.limits.rightBound.push_back({clickLat, clickLon}); }
                    }
                    else if (currentMode == TrackEditorMode::PlaceFinishLine || currentMode == TrackEditorMode::PlaceSector) {
                        bool flippedArrow = false;
                        if (!isPlacingFence) {
                            for (auto& fence : activeTrack.fences) {
                                ImVec2 p1 = mapView.GetScreenPos(fence.p1.lat, fence.p1.lon, canvasCenter);
                                ImVec2 p2 = mapView.GetScreenPos(fence.p2.lat, fence.p2.lon, canvasCenter);
                                ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
                                if (std::hypot(mousePos.x - mid.x, mousePos.y - mid.y) < 20.0f) {
                                    trySaveState(); fence.directionHeading = std::fmod(fence.directionHeading + M_PI, 2.0 * M_PI); flippedArrow = true; break;
                                }
                            }
                        }

                        if (!flippedArrow) {
                            if (!isPlacingFence) {
                                if (currentMode == TrackEditorMode::PlaceFinishLine) {
                                    bool hasFinishLine = false;
                                    for (const auto& f : activeTrack.fences) if (f.type == TrackFenceType::FinishLine) hasFinishLine = true;
                                    if (hasFinishLine) return; 
                                }
                                tempFenceLat = clickLat; tempFenceLon = clickLon;
                                isPlacingFence = true;
                            } else {
                                trySaveState();
                                TrackFence newFence{};
                                if (currentMode == TrackEditorMode::PlaceFinishLine) {
                                    newFence.name = "Finish Line"; newFence.type = TrackFenceType::FinishLine;
                                } else {
                                    int sectorCount = 1;
                                    for (const auto& f : activeTrack.fences) if (f.type == TrackFenceType::Sector) sectorCount++;
                                    newFence.name = "Sector " + std::to_string(sectorCount); newFence.type = TrackFenceType::Sector;
                                }
                                newFence.p1.lat = tempFenceLat; newFence.p1.lon = tempFenceLon;
                                newFence.p2.lat = clickLat; newFence.p2.lon = clickLon;
                                
                                // --- FIX COMPENSAZIONE LATITUDINE CREAZIONE RECINZIONE ---
                                double latCenterRad = tempFenceLat * M_PI / 180.0;
                                double dx = (clickLon - tempFenceLon) * std::cos(latCenterRad);
                                double dy = clickLat - tempFenceLat;
                                newFence.directionHeading = std::atan2(dy, dx) + (M_PI / 2.0);
                                // ---------------------------------------------------------
                                
                                activeTrack.fences.push_back(newFence);
                                isPlacingFence = false;
                            }
                        }
                    }
                }
            }
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && draggedPointType != DraggedPointType::None) {
            auto applyDragSnap = [&](const auto& bound, DraggedPointType type) {
                if (draggedPointType == type && bound.size() > 2 && draggedPointIndex == bound.size() - 1) {
                    ImVec2 firstPt = mapView.GetScreenPos(bound[0].lat, bound[0].lon, canvasCenter);
                    if (std::hypot(mousePos.x - firstPt.x, mousePos.y - firstPt.y) < snapDist) { clickLat = bound[0].lat; clickLon = bound[0].lon; }
                }
            };
            applyDragSnap(activeTrack.limits.leftBound, DraggedPointType::LeftBound);
            applyDragSnap(activeTrack.limits.rightBound, DraggedPointType::RightBound);

            auto updateBoundDrag = [&](auto& bound, DraggedPointType type) {
                if (draggedPointType == type) {
                    bool isClosed = GeoMath::IsBoundClosed(bound);
                    bound[draggedPointIndex] = {clickLat, clickLon};
                    if (isClosed) {
                        if (draggedPointIndex == 0) bound.back() = {clickLat, clickLon};
                        else if (draggedPointIndex == bound.size() - 1) bound.front() = {clickLat, clickLon};
                    }
                }
            };
            updateBoundDrag(activeTrack.limits.leftBound, DraggedPointType::LeftBound);
            updateBoundDrag(activeTrack.limits.rightBound, DraggedPointType::RightBound);

            if (draggedPointType == DraggedPointType::FenceP1 || draggedPointType == DraggedPointType::FenceP2) {
                auto& f = activeTrack.fences[draggedPointIndex];
                if (draggedPointType == DraggedPointType::FenceP1) f.p1 = {clickLat, clickLon};
                else f.p2 = {clickLat, clickLon};
                
                // --- FIX COMPENSAZIONE LATITUDINE TRASCINAMENTO RECINZIONE ---
                double latCenterRad = f.p1.lat * M_PI / 180.0;
                double dx = (f.p2.lon - f.p1.lon) * std::cos(latCenterRad); 
                double dy = f.p2.lat - f.p1.lat;
                f.directionHeading = std::atan2(dy, dx) + (M_PI / 2.0);
                // -------------------------------------------------------------
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) draggedPointType = DraggedPointType::None;
    }
}

void TrackManagerModal::RenderTrackOverlays(ImDrawList* drawList, ImVec2 canvasCenter, bool isMapHovered) {
    ImVec2 mousePos = ImGui::GetMousePos();
    float snapDist = 15.0f;

    auto drawBound = [&](const auto& bound, ImU32 color, TrackEditorMode mode) {
        for (size_t i = 1; i < bound.size(); ++i) {
            ImVec2 p1 = mapView.GetScreenPos(bound[i-1].lat, bound[i-1].lon, canvasCenter);
            ImVec2 p2 = mapView.GetScreenPos(bound[i].lat, bound[i].lon, canvasCenter);
            drawList->AddLine(p1, p2, color, 2.0f);
        }
        if (currentMode == mode && !bound.empty() && isMapHovered && !GeoMath::IsBoundClosed(bound)) {
            ImVec2 p1 = mapView.GetScreenPos(bound.back().lat, bound.back().lon, canvasCenter);
            ImVec2 p2 = mousePos;
            if (bound.size() > 2) {
                ImVec2 firstPt = mapView.GetScreenPos(bound[0].lat, bound[0].lon, canvasCenter);
                if (std::hypot(mousePos.x - firstPt.x, mousePos.y - firstPt.y) < snapDist) p2 = firstPt; 
            }
            drawList->AddLine(p1, p2, IM_COL32(255, 255, 255, 150), 2.0f);
        }
    };

    drawBound(activeTrack.limits.leftBound, IM_COL32(255, 50, 50, 255), TrackEditorMode::DrawLeftBound);
    drawBound(activeTrack.limits.rightBound, IM_COL32(50, 50, 255, 255), TrackEditorMode::DrawRightBound);

    for (const auto& fence : activeTrack.fences) {
        ImVec2 p1 = mapView.GetScreenPos(fence.p1.lat, fence.p1.lon, canvasCenter);
        ImVec2 p2 = mapView.GetScreenPos(fence.p2.lat, fence.p2.lon, canvasCenter);
        ImU32 fenceColor = (fence.type == TrackFenceType::FinishLine) ? IM_COL32(50, 255, 50, 255) : IM_COL32(255, 150, 0, 255);
        drawList->AddLine(p1, p2, fenceColor, 4.0f);

        ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
        float screenAngle = -fence.directionHeading; 
        float arrowLen = 20.0f;
        ImVec2 arrowEnd(mid.x + cos(screenAngle) * arrowLen, mid.y + sin(screenAngle) * arrowLen);
        
        ImU32 arrowColor = IM_COL32(255, 255, 255, 255);
        drawList->AddLine(mid, arrowEnd, arrowColor, 3.0f);
        
        float headLen = 8.0f;
        float angle1 = screenAngle + M_PI * 0.85f;
        float angle2 = screenAngle - M_PI * 0.85f;
        drawList->AddLine(arrowEnd, ImVec2(arrowEnd.x + cos(angle1) * headLen, arrowEnd.y + sin(angle1) * headLen), arrowColor, 3.0f);
        drawList->AddLine(arrowEnd, ImVec2(arrowEnd.x + cos(angle2) * headLen, arrowEnd.y + sin(angle2) * headLen), arrowColor, 3.0f);

        if ((currentMode == TrackEditorMode::PlaceFinishLine || currentMode == TrackEditorMode::PlaceSector) && isMapHovered) {
            if (std::hypot(mousePos.x - mid.x, mousePos.y - mid.y) < 20.0f) drawList->AddCircleFilled(mid, 12.0f, IM_COL32(255, 255, 255, 100));
        }
    }
    
    if ((currentMode == TrackEditorMode::PlaceFinishLine || currentMode == TrackEditorMode::PlaceSector) && isPlacingFence && isMapHovered) {
        ImVec2 p1 = mapView.GetScreenPos(tempFenceLat, tempFenceLon, canvasCenter);
        ImU32 previewColor = (currentMode == TrackEditorMode::PlaceFinishLine) ? IM_COL32(50, 255, 50, 150) : IM_COL32(255, 150, 0, 150);
        drawList->AddLine(p1, mousePos, previewColor, 4.0f);
    }

    bool hoverDrawn = false;
    if (isMapHovered && currentMode != TrackEditorMode::ViewOnly) {
        bool isDeleting = (currentMode == TrackEditorMode::DeletePoint);
        auto drawDot = [&](ImVec2 pt) {
            ImU32 color = isDeleting ? IM_COL32(255, 50, 50, 200) : IM_COL32(255, 255, 255, 150);
            drawList->AddCircleFilled(pt, 8.0f, color);
            hoverDrawn = true;
        };

        auto checkExactPoint = [&](const auto& bound) {
            if (hoverDrawn) return;
            for (const auto& pt : bound) {
                ImVec2 screenPt = mapView.GetScreenPos(pt.lat, pt.lon, canvasCenter);
                if (std::hypot(mousePos.x - screenPt.x, mousePos.y - screenPt.y) < snapDist) { drawDot(screenPt); return; }
            }
        };

        if (currentMode == TrackEditorMode::DrawLeftBound || isDeleting) checkExactPoint(activeTrack.limits.leftBound);
        if (currentMode == TrackEditorMode::DrawRightBound || isDeleting) checkExactPoint(activeTrack.limits.rightBound);
        
        if (currentMode == TrackEditorMode::PlaceFinishLine || currentMode == TrackEditorMode::PlaceSector || isDeleting) {
            TrackFenceType fType = (currentMode == TrackEditorMode::PlaceFinishLine) ? TrackFenceType::FinishLine : TrackFenceType::Sector;
            for (const auto& fence : activeTrack.fences) {
                if (isDeleting || fence.type == fType) {
                    ImVec2 p1 = mapView.GetScreenPos(fence.p1.lat, fence.p1.lon, canvasCenter);
                    if (!hoverDrawn && std::hypot(mousePos.x - p1.x, mousePos.y - p1.y) < snapDist) drawDot(p1);
                    ImVec2 p2 = mapView.GetScreenPos(fence.p2.lat, fence.p2.lon, canvasCenter);
                    if (!hoverDrawn && std::hypot(mousePos.x - p2.x, mousePos.y - p2.y) < snapDist) drawDot(p2);
                    
                    if (isDeleting) {
                        ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
                        if (!hoverDrawn && std::hypot(mousePos.x - mid.x, mousePos.y - mid.y) < 20.0f) drawDot(mid);
                    }
                }
            }
        }

        if (!hoverDrawn && !isDeleting) {
            auto checkLineSplit = [&](const auto& bound) {
                if (bound.size() < 2) return;
                for (size_t i = 0; i < bound.size() - 1; ++i) {
                    ImVec2 p1 = mapView.GetScreenPos(bound[i].lat, bound[i].lon, canvasCenter);
                    ImVec2 p2 = mapView.GetScreenPos(bound[i+1].lat, bound[i+1].lon, canvasCenter);
                    if (GeoMath::DistancePointToSegment(mousePos, p1, p2) < snapDist) {
                        float l2 = std::pow(p1.x - p2.x, 2) + std::pow(p1.y - p2.y, 2);
                        float t = std::max(0.0f, std::min(1.0f, ((mousePos.x - p1.x) * (p2.x - p1.x) + (mousePos.y - p1.y) * (p2.y - p1.y)) / l2));
                        ImVec2 proj = { p1.x + t * (p2.x - p1.x), p1.y + t * (p2.y - p1.y) };
                        drawDot(proj); break;
                    }
                }
            };
            if (currentMode == TrackEditorMode::DrawLeftBound) checkLineSplit(activeTrack.limits.leftBound);
            else if (currentMode == TrackEditorMode::DrawRightBound) checkLineSplit(activeTrack.limits.rightBound);
        }
    }
}

void TrackManagerModal::RenderLibraryColumn() {
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "TRACKS");
    ImGui::Separator();
    
    ImGui::InputTextWithHint("##SearchTrack", "Search...", searchBuffer, sizeof(searchBuffer));
    ImGui::Dummy(ImVec2(0, 2.0f));
    ImGui::Separator();

    std::string searchStr(searchBuffer);
    std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

    float buttonHeight = 35.0f * ImGui::GetIO().FontGlobalScale;
    if (ImGui::BeginChild("TrackListScroll", ImVec2(0, ImGui::GetContentRegionAvail().y - buttonHeight))) {
        for (const auto& tName : savedTracks) {
            if (!searchStr.empty()) {
                std::string tNameLower = tName;
                std::transform(tNameLower.begin(), tNameLower.end(), tNameLower.begin(), ::tolower);
                if (tNameLower.find(searchStr) == std::string::npos) continue;
            }

            bool isSelected = (currentFileName == tName);
            if (ImGui::Selectable(tName.c_str(), isSelected)) {
                if (isTrackActive && isTrackModified && currentFileName != tName) {
                    // Blocca il caricamento e apri il popup
                    showUnsavedChangesPopup = true;
                    pendingTrackLoad = tName; 
                } else if (currentFileName != tName) { // Carica normalmente
                    currentFileName = tName;
                    isTrackActive = true;
                    isPlacingFence = false;
                    isTrackModified = false; // Reset
                    std::string path = std::string(PROJECT_ROOT_DIR) + "/assets/tracks/" + tName + ".json";
                    TrackSerializer::LoadTrack(path, activeTrack);
                    needsMapCentering = true;
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (ImGui::Button("Create New", ImVec2(-1, 0))) {
        if (isTrackActive && isTrackModified) {
            // Usa una parola chiave speciale per riconoscere la creazione di una nuova pista
            showUnsavedChangesPopup = true;
            pendingTrackLoad = "##NEW_TRACK##";
        } else {
            activeTrack = Track();
            activeTrack.name = GetUniqueTrackName("NewTrack");
            currentFileName = "";
            isTrackActive = true;
            isPlacingFence = false;
            isTrackModified = false; // Reset
            trackHistory.clear(); 
        }
    }
}

void TrackManagerModal::RenderToolsColumn() {
    std::string importPathCopy;
    std::string exportPathCopy;
    {
        std::lock_guard<std::mutex> lock(*dialogMutex);;
        if (triggerImport) { importPathCopy = pendingImportPath; triggerImport = false; pendingImportPath.clear(); }
        if (triggerExport) { exportPathCopy = pendingExportPath; triggerExport = false; pendingExportPath.clear(); }
    }

    if (!importPathCopy.empty()) {
        TrackSerializer::LoadTrack(importPathCopy, activeTrack);
        std::filesystem::path p(importPathCopy);
        std::string extractedName = p.stem().string(); 
        
        if (!extractedName.empty()) activeTrack.name = extractedName;
        else if (activeTrack.name.empty()) activeTrack.name = "ImportedTrack";
        
        activeTrack.name = GetUniqueTrackName(activeTrack.name); 

        std::string localPath = std::string(PROJECT_ROOT_DIR) + "/assets/tracks/" + activeTrack.name + ".json";
        TrackSerializer::SaveTrack(activeTrack, localPath);
        
        currentFileName = activeTrack.name;
        isTrackActive = true;
        isPlacingFence = false;
        isTrackModified = false;
        RefreshTrackList(); 
        needsMapCentering = true;
    }
    
    if (!exportPathCopy.empty()) {
        if (exportPathCopy.length() < 5 || exportPathCopy.substr(exportPathCopy.length() - 5) != ".json") exportPathCopy += ".json";
        TrackSerializer::SaveTrack(activeTrack, exportPathCopy);
    }

    ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "PROPERTIES");
    ImGui::Separator();

    char buf[64];
    memset(buf, 0, sizeof(buf));
    strncpy(buf, activeTrack.name.c_str(), sizeof(buf) - 1);
    if (ImGui::InputText("Name", buf, sizeof(buf))){
        activeTrack.name = buf;
        isTrackModified = true;
    }

    // --- CALCOLO LUNGHEZZA PISTA ---
    // Lambda per la formula di Haversine (calcola la distanza in metri tra due coordinate GPS)
    auto calculateDistance = [](double lat1, double lon1, double lat2, double lon2) {
        constexpr double R = 6371000.0; // Raggio della Terra in metri
        double dLat = (lat2 - lat1) * M_PI / 180.0;
        double dLon = (lon2 - lon1) * M_PI / 180.0;
        double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                   std::cos(lat1 * M_PI / 180.0) * std::cos(lat2 * M_PI / 180.0) *
                   std::sin(dLon / 2) * std::sin(dLon / 2);
        return R * 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    };

    double trackLength = 0.0;
    
    if (!activeTrack.limits.leftBound.empty() && !activeTrack.limits.rightBound.empty()) {
        std::vector<std::pair<double, double>> centerLine;
        
        // Generazione della centerline
        for (const auto& lPt : activeTrack.limits.leftBound) {
            double minDist = std::numeric_limits<double>::max();
            double bestLat = lPt.lat, bestLon = lPt.lon;
            
            for (const auto& rPt : activeTrack.limits.rightBound) {
                // Distanza euclidea rapida per trovare il punto più vicino sul bordo opposto
                double dist = std::hypot(lPt.lat - rPt.lat, lPt.lon - rPt.lon);
                if (dist < minDist) {
                    minDist = dist;
                    bestLat = rPt.lat;
                    bestLon = rPt.lon;
                }
            }
            // Punto medio tra il bordo sinistro e il punto più vicino del bordo destro
            centerLine.push_back({(lPt.lat + bestLat) / 2.0, (lPt.lon + bestLon) / 2.0});
        }
        
        // Somma delle distanze lungo la centerline
        for (size_t i = 1; i < centerLine.size(); ++i) {
            trackLength += calculateDistance(centerLine[i-1].first, centerLine[i-1].second, centerLine[i].first, centerLine[i].second);
        }
        
        // Se la pista è chiusa (circuito), calcoliamo anche l'ultimo segmento che chiude il loop
        if (GeoMath::IsBoundClosed(activeTrack.limits.leftBound) && centerLine.size() > 2) {
            trackLength += calculateDistance(centerLine.back().first, centerLine.back().second, centerLine.front().first, centerLine.front().second);
        }
    } else {
        // Fallback: se l'utente ha disegnato un solo bordo, usiamo quello per dare una stima della lunghezza
        const auto& bound = activeTrack.limits.leftBound.empty() ? activeTrack.limits.rightBound : activeTrack.limits.leftBound;
        for (size_t i = 1; i < bound.size(); ++i) {
            trackLength += calculateDistance(bound[i-1].lat, bound[i-1].lon, bound[i].lat, bound[i].lon);
        }
        if (GeoMath::IsBoundClosed(bound) && bound.size() > 2) {
            trackLength += calculateDistance(bound.back().lat, bound.back().lon, bound.front().lat, bound.front().lon);
        }
    }

    ImGui::Dummy(ImVec2(0, 2.0f));
    ImGui::Text("Length:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%.0f m", trackLength);

    activeTrack.lengthMeters = trackLength;
    // -------------------------------

    // Spostiamo la logica di salvataggio qui in alto per poterla richiamare con Ctrl+S
    auto saveCurrentTrack = [&]() {
        if (!currentFileName.empty() && currentFileName != activeTrack.name) {
            std::string oldPath = std::string(PROJECT_ROOT_DIR) + "/assets/tracks/" + currentFileName + ".json";
            if (std::filesystem::exists(oldPath)) std::filesystem::remove(oldPath);
        }
        std::string path = std::string(PROJECT_ROOT_DIR) + "/assets/tracks/" + activeTrack.name + ".json";
        TrackSerializer::SaveTrack(activeTrack, path);
        currentFileName = activeTrack.name;
        isTrackModified = false;
        RefreshTrackList();
    };

    // --- SHORTCUT DA TASTIERA ---
    ImGuiIO& io = ImGui::GetIO();
    // Eseguiamo gli shortcut SOLO se non stiamo scrivendo in un InputText (es. il nome della pista o la barra di ricerca)
    if (!io.WantTextInput) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) PerformUndo();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            if (!activeTrack.name.empty()) {
                bool nameExists = std::find(savedTracks.begin(), savedTracks.end(), activeTrack.name) != savedTracks.end();
                if (nameExists && activeTrack.name != currentFileName) showOverwritePopup = true;
                else saveCurrentTrack();
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) currentMode = TrackEditorMode::ViewOnly;
        if (ImGui::IsKeyPressed(ImGuiKey_1, false)) currentMode = TrackEditorMode::DrawLeftBound;
        if (ImGui::IsKeyPressed(ImGuiKey_2, false)) currentMode = TrackEditorMode::DrawRightBound;
        if (ImGui::IsKeyPressed(ImGuiKey_3, false)) currentMode = TrackEditorMode::PlaceFinishLine;
        if (ImGui::IsKeyPressed(ImGuiKey_4, false)) currentMode = TrackEditorMode::PlaceSector;
        if (ImGui::IsKeyPressed(ImGuiKey_5, false)) currentMode = TrackEditorMode::DeletePoint;
    }
    // ----------------------------

    ImGui::Dummy(ImVec2(0, 15.0f));
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "TOOLS");
    ImGui::Separator();

    int mode = (int)currentMode;
    ImGui::RadioButton("Navigate", &mode, (int)TrackEditorMode::ViewOnly);
    ImGui::RadioButton("Draw Left Bound", &mode, (int)TrackEditorMode::DrawLeftBound);
    ImGui::RadioButton("Draw Right Bound", &mode, (int)TrackEditorMode::DrawRightBound);
    ImGui::RadioButton("Place Finish Line", &mode, (int)TrackEditorMode::PlaceFinishLine);
    ImGui::RadioButton("Place Sector", &mode, (int)TrackEditorMode::PlaceSector);
    ImGui::RadioButton("Delete Points", &mode, (int)TrackEditorMode::DeletePoint);
    currentMode = (TrackEditorMode)mode;

    ImGui::Dummy(ImVec2(0, 5.0f));
    if (ImGui::Button("Undo Last Action", ImVec2(-1, 0))) PerformUndo();

    ImGui::Dummy(ImVec2(0, ImGui::GetContentRegionAvail().y - 110.0f * ImGui::GetIO().FontGlobalScale));
    ImGui::Separator();
    
    if (ImGui::Button("Save to Library", ImVec2(-1, 0))) {
        if (!activeTrack.name.empty()) {
            bool nameExists = std::find(savedTracks.begin(), savedTracks.end(), activeTrack.name) != savedTracks.end();
            if (nameExists && activeTrack.name != currentFileName) showOverwritePopup = true;
            else saveCurrentTrack();
        }
    }

    ImGui::Dummy(ImVec2(0, 5.0f));
    if (ImGui::Button("Import JSON...", ImVec2(-1, 0))) {
        if (isTrackActive && isTrackModified) {
            showUnsavedChangesPopup = true;
            pendingTrackLoad = "##IMPORT_TRACK##";
        } else {
            FileDialog::OpenFile(nullptr, [this](const std::string& path) {
                if (!path.empty()) { std::lock_guard<std::mutex> lock(*dialogMutex); pendingImportPath = path; triggerImport = true; }
            });
        }
    }

    if (ImGui::Button("Export JSON...", ImVec2(-1, 0))) {
        if (!activeTrack.name.empty()) {
            std::string defaultExportName = activeTrack.name + ".json"; 
            
            FileDialog::SaveFile(nullptr, defaultExportName, [this](const std::string& path) {
                if (!path.empty()) { 
                    std::lock_guard<std::mutex> lock(*dialogMutex); 
                    pendingExportPath = path; 
                    triggerExport = true; 
                }
            });
        }
    }

    ImGui::Dummy(ImVec2(0, 10.0f));
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button("Delete Track from Library", ImVec2(-1, 0))) {
        if (!currentFileName.empty()) showDeletePopup = true;
    }
    ImGui::PopStyleColor(3);

    if (showOverwritePopup) ImGui::OpenPopup("Overwrite Warning");
    if (ImGui::BeginPopupModal("Overwrite Warning", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("A track named '%s' already exists.\nDo you want to overwrite it?", activeTrack.name.c_str());
        ImGui::Separator();
        if (ImGui::Button("Yes, Overwrite", ImVec2(120, 0))) {
            saveCurrentTrack();
            showOverwritePopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { showOverwritePopup = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    if (showDeletePopup) ImGui::OpenPopup("Delete Warning");
    if (ImGui::BeginPopupModal("Delete Warning", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to permanently delete '%s'?\nThis action cannot be undone.", currentFileName.c_str());
        ImGui::Separator();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
            std::string path = std::string(PROJECT_ROOT_DIR) + "/assets/tracks/" + currentFileName + ".json";
            if (std::filesystem::exists(path)) std::filesystem::remove(path);
            
            activeTrack = Track();
            currentFileName = "";
            isTrackActive = false;
            trackHistory.clear();
            RefreshTrackList();
            needsMapCentering = true;
            
            showDeletePopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { showDeletePopup = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    if (showUnsavedChangesPopup) ImGui::OpenPopup("Unsaved Changes");
    if (ImGui::BeginPopupModal("Unsaved Changes", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes in the current track.\nIf you continue, these changes will be lost.");
        ImGui::Separator();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Discard Changes", ImVec2(140, 0))) {
            if (pendingTrackLoad == "##NEW_TRACK##") {
                activeTrack = Track();
                activeTrack.name = GetUniqueTrackName("NewTrack");
                currentFileName = "";
                isTrackActive = true;
                isPlacingFence = false;
                isTrackModified = false;
                trackHistory.clear();
            } else if (pendingTrackLoad == "##IMPORT_TRACK##") {
                FileDialog::OpenFile(nullptr, [this](const std::string& path) {
                    if (!path.empty()) { std::lock_guard<std::mutex> lock(*dialogMutex); pendingImportPath = path; triggerImport = true; }
                });
            } else if (pendingTrackLoad == "##CLOSE_MANAGER##") {
                if (!currentFileName.empty()) {
                    std::string path = std::string(PROJECT_ROOT_DIR) + "/assets/tracks/" + currentFileName + ".json";
                    TrackSerializer::LoadTrack(path, activeTrack);
                } else {
                    activeTrack = Track(); 
                }
                isTrackModified = false;
                isOpen = false; 
            } else {
                currentFileName = pendingTrackLoad;
                isTrackActive = true;
                isPlacingFence = false;
                isTrackModified = false;
                std::string path = std::string(PROJECT_ROOT_DIR) + "/assets/tracks/" + pendingTrackLoad + ".json";
                TrackSerializer::LoadTrack(path, activeTrack);
                needsMapCentering = true;
            }
            showUnsavedChangesPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { 
            showUnsavedChangesPopup = false; 
            ImGui::CloseCurrentPopup(); 
        }
        ImGui::EndPopup();
    }
}

void TrackManagerModal::SaveStateForUndo() {
    trackHistory.push_back(activeTrack);
    if (trackHistory.size() > 50) trackHistory.erase(trackHistory.begin());
    isTrackModified = true;
}

void TrackManagerModal::PerformUndo() {
    if ((currentMode == TrackEditorMode::PlaceFinishLine || currentMode == TrackEditorMode::PlaceSector) && isPlacingFence) {
        isPlacingFence = false; return;
    }
    if (!trackHistory.empty()) {
        activeTrack = trackHistory.back();
        trackHistory.pop_back();
        draggedPointType = DraggedPointType::None; 
    }
}