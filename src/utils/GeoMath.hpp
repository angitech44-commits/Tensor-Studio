#pragma once
#include <imgui.h>
#include "../core/Track.hpp"
#include <vector>

namespace GeoMath {
    // Proietta Lat/Lon in coordinate schermo X/Y
    ImVec2 LatLonToScreen(double lat, double lon, double centerLat, double centerLon, float zoom, ImVec2 canvasCenter);
    
    // Calcola la distanza minima tra un punto e un segmento
    float DistancePointToSegment(ImVec2 p, ImVec2 v, ImVec2 w);
    
    // Verifica se un vettore di GeoPoint forma un loop chiuso
    bool IsBoundClosed(const std::vector<GeoPoint>& bound);

    void CalculateTrackCenterAndZoom(const Track& track, ImVec2 canvasSize, float paddingPct, double& outCenterLat, double& outCenterLon, float& outZoom);
}