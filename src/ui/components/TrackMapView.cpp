#include "TrackMapView.hpp"
#include "../../utils/GeoMath.hpp"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TrackMapView::TrackMapView() {
    tileManager = std::make_unique<TileManager>(4);
}

void TrackMapView::CenterOn(double lat, double lon, float zoom) {
    mapCenterLat = lat;
    mapCenterLon = lon;
    mapZoom = zoom;
}

ImVec2 TrackMapView::GetScreenPos(double lat, double lon, ImVec2 canvasCenter) const {
    return GeoMath::LatLonToScreen(lat, lon, mapCenterLat, mapCenterLon, mapZoom, canvasCenter);
}

void TrackMapView::RenderBaseMap(SDL_Renderer* renderer, ImVec2 canvasPos, ImVec2 canvasSize, bool allowPanZoom, bool& outIsHovered, bool& outIsActive) {
    ImVec2 canvasCenter(canvasPos.x + canvasSize.x * 0.5f, canvasPos.y + canvasSize.y * 0.5f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(30, 30, 35, 255));
    tileManager->ProcessCompletedDownloads(renderer);

    // IL FIX: Creiamo SEMPRE la hitbox per catturare il mouse, per QUALSIASI strumento
    ImGui::SetNextItemAllowOverlap(); 
    ImGui::InvisibleButton("##MapInteract", canvasSize);
    
    outIsHovered = ImGui::IsItemHovered();
    outIsActive = ImGui::IsItemActive();

    if (outIsHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) isMiddleDragging = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) isMiddleDragging = false;

    // Se stiamo usando gli strumenti, blocchiamo il pan col tasto sinistro, ma lasciamo quello col tasto centrale
    bool isPanMMB = isMiddleDragging;
    bool isPanLMB = (allowPanZoom && outIsActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left));
    bool isScrolling = (outIsHovered && ImGui::GetIO().MouseWheel != 0);

    if (isPanMMB || isPanLMB || isScrolling) lastMapInteractionTime = ImGui::GetTime();
    if (isScrolling) mapZoom = std::max(2.0f, std::min(21.0f, mapZoom + ImGui::GetIO().MouseWheel * 0.5f));

    if (isPanMMB || isPanLMB) {
        double fullScale = 256.0 * std::pow(2.0, mapZoom);
        double latRad = mapCenterLat * M_PI / 180.0;
        double cX_global = ((mapCenterLon + 180.0) / 360.0) * fullScale;
        double cY_global = ((1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0) * fullScale;

        ImVec2 delta = ImGui::GetIO().MouseDelta;
        double cX = cX_global - delta.x;
        double cY = cY_global - delta.y;
        
        mapCenterLon = (cX / fullScale) * 360.0 - 180.0;
        double nY = 1.0 - 2.0 * (cY / fullScale);
        mapCenterLat = std::atan(std::sinh(nY * M_PI)) * 180.0 / M_PI;
    }

    drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);
    RenderTiles(drawList, canvasCenter, canvasSize);
}

void TrackMapView::RenderTiles(ImDrawList* drawList, ImVec2 canvasCenter, ImVec2 canvasSize) {
    int max_esri_zoom = 19; 
    int request_z = std::min((int)std::floor(mapZoom), max_esri_zoom); 
    
    double powZ = std::pow(2.0, request_z); 
    double fullScale = 256.0 * std::pow(2.0, mapZoom); 
    double tSize_global = (1.0 / powZ) * fullScale;    
    
    auto lonToX = [](double lon) { return (lon + 180.0) / 360.0; };
    auto latToY = [](double lat) {
        double latRad = lat * M_PI / 180.0;
        return (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0;
    };
    
    double cX_global = lonToX(mapCenterLon) * fullScale;
    double cY_global = latToY(mapCenterLat) * fullScale;
    double centerTx = lonToX(mapCenterLon) * powZ;
    double centerTy = latToY(mapCenterLat) * powZ;
    
    int startX = (int)std::floor(centerTx - (canvasSize.x / 2.0) / tSize_global);
    int endX   = (int)std::floor(centerTx + (canvasSize.x / 2.0) / tSize_global);
    int startY = (int)std::floor(centerTy - (canvasSize.y / 2.0) / tSize_global);
    int endY   = (int)std::floor(centerTy + (canvasSize.y / 2.0) / tSize_global);
    
    int maxTile = (int)powZ - 1;
    startY = std::max(0, std::min(startY, maxTile));
    endY = std::max(0, std::min(endY, maxTile));
    
    for (int ty = startY; ty <= endY; ++ty) {
        for (int tx = startX; tx <= endX; ++tx) {
            int wrappedTx = tx % (maxTile + 1);
            if (wrappedTx < 0) wrappedTx += (maxTile + 1);

            double tX_global = (tx / powZ) * fullScale;
            double tY_global = (ty / powZ) * fullScale;

            ImVec2 pMin(canvasCenter.x + (float)(tX_global - cX_global), canvasCenter.y + (float)(tY_global - cY_global));
            ImVec2 pMax(pMin.x + (float)tSize_global, pMin.y + (float)tSize_global);

            bool canRequestTiles = (ImGui::GetTime() - lastMapInteractionTime) > 0.15f;

            TileKey keySat = {request_z, wrappedTx, ty, 0};
            SDL_Texture* texSat = tileManager->GetTexture(keySat);
            if (texSat) {
                drawList->AddImage((ImTextureID)(intptr_t)texSat, pMin, pMax);
            } else {
                if (canRequestTiles) tileManager->RequestTile(keySat);
                drawList->AddRect(pMin, pMax, IM_COL32(50, 50, 60, 255));
            }

            TileKey keyLabels = {request_z, wrappedTx, ty, 1};
            SDL_Texture* texLabels = tileManager->GetTexture(keyLabels);
            if (texLabels) {
                drawList->AddImage((ImTextureID)(intptr_t)texLabels, pMin, pMax);
            } else {
                if (canRequestTiles) tileManager->RequestTile(keyLabels);
            }
        }
    }
}

void TrackMapView::RenderUIControls(ImVec2 canvasPos, ImVec2 canvasSize) {
    ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + 10, canvasPos.y + 10));
    if (ImGui::Button("+", ImVec2(40, 40))) mapZoom = std::min(21.0f, mapZoom + 1.0f);
    
    ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + 10, canvasPos.y + 55));
    if (ImGui::Button("-", ImVec2(40, 40))) mapZoom = std::max(2.0f, mapZoom - 1.0f);
    
    ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + 5, canvasPos.y + canvasSize.y - 20));
    ImGui::TextDisabled("Imagery © Esri");
}