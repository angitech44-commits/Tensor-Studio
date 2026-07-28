#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>
#include <memory>
#include "../../utils/TileManager.hpp"

class TrackMapView {
public:
    double mapCenterLat = 45.6201;
    double mapCenterLon = 9.2812;
    float mapZoom = 17.0f;

    TrackMapView();
    
    // NUOVA FIRMA: Ora la mappa restituisce al Modal se il mouse è sopra di essa
    void RenderBaseMap(SDL_Renderer* renderer, ImVec2 canvasPos, ImVec2 canvasSize, bool allowPanZoom, bool& outIsHovered, bool& outIsActive);
    
    void RenderUIControls(ImVec2 canvasPos, ImVec2 canvasSize);
    void CenterOn(double lat, double lon, float zoom);
    ImVec2 GetScreenPos(double lat, double lon, ImVec2 canvasCenter) const;

private:
    std::unique_ptr<TileManager> tileManager;
    bool isMiddleDragging = false;
    float lastMapInteractionTime = 0.0f;

    void RenderTiles(ImDrawList* drawList, ImVec2 canvasCenter, ImVec2 canvasSize);
};