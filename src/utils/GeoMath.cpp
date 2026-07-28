#include "GeoMath.hpp"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace GeoMath {

    ImVec2 LatLonToScreen(double lat, double lon, double centerLat, double centerLon, float zoom, ImVec2 canvasCenter) {
        double scale = 256.0 * std::pow(2.0, zoom);
        
        auto lonToX = [](double lon) { return (lon + 180.0) / 360.0; };
        auto latToY = [](double lat) {
            double latRad = lat * M_PI / 180.0;
            return (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0;
        };

        double centerX = lonToX(centerLon) * scale;
        double centerY = latToY(centerLat) * scale;
        double px = lonToX(lon) * scale;
        double py = latToY(lat) * scale;

        return ImVec2(canvasCenter.x + (float)(px - centerX), canvasCenter.y + (float)(py - centerY));
    }

    float DistancePointToSegment(ImVec2 p, ImVec2 v, ImVec2 w) {
        float l2 = std::pow(v.x - w.x, 2) + std::pow(v.y - w.y, 2);
        if (l2 == 0.0f) return std::hypot(p.x - v.x, p.y - v.y);
        float t = std::max(0.0f, std::min(1.0f, ((p.x - v.x) * (w.x - v.x) + (p.y - v.y) * (w.y - v.y)) / l2));
        ImVec2 projection = { v.x + t * (w.x - v.x), v.y + t * (w.y - v.y) };
        return std::hypot(p.x - projection.x, p.y - projection.y);
    }

    bool IsBoundClosed(const std::vector<GeoPoint>& bound) {
        if (bound.size() > 2) {
            return (bound.front().lat == bound.back().lat && bound.front().lon == bound.back().lon);
        }
        return false;
    }

    void CalculateTrackCenterAndZoom(const Track& track, ImVec2 canvasSize, float paddingPct, double& outCenterLat, double& outCenterLon, float& outZoom) {
        double minLat = 90.0, maxLat = -90.0;
        double minLon = 180.0, maxLon = -180.0;
        bool hasPoints = false;

        auto updateBounds = [&](const std::vector<GeoPoint>& bound) {
            for (const auto& pt : bound) {
                if (pt.lat < minLat) minLat = pt.lat;
                if (pt.lat > maxLat) maxLat = pt.lat;
                if (pt.lon < minLon) minLon = pt.lon;
                if (pt.lon > maxLon) maxLon = pt.lon;
                hasPoints = true;
            }
        };

        updateBounds(track.limits.leftBound);
        updateBounds(track.limits.rightBound);
        
        for (const auto& f : track.fences) {
            if (f.p1.lat < minLat) minLat = f.p1.lat;
            if (f.p1.lat > maxLat) maxLat = f.p1.lat;
            if (f.p1.lon < minLon) minLon = f.p1.lon;
            if (f.p1.lon > maxLon) maxLon = f.p1.lon;
            hasPoints = true;
        }

        if (!hasPoints || canvasSize.x <= 0 || canvasSize.y <= 0) {
            outCenterLat = 45.6201; 
            outCenterLon = 9.2812;
            outZoom = 17.0f;
            return;
        }

        // Il centro geografico
        outCenterLat = (minLat + maxLat) / 2.0;
        outCenterLon = (minLon + maxLon) / 2.0;

        // Se è un punto singolo
        if (minLat == maxLat && minLon == maxLon) {
            outZoom = 19.0f; 
            return;
        }

        auto latToY = [](double lat) {
            double latRad = lat * M_PI / 180.0;
            return (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0;
        };
        auto lonToX = [](double lon) { return (lon + 180.0) / 360.0; };

        // Delta geografico in proiezione di Mercatore
        double dx = std::abs(lonToX(maxLon) - lonToX(minLon));
        double dy = std::abs(latToY(maxLat) - latToY(minLat));

        if (dx == 0) dx = 0.00001;
        if (dy == 0) dy = 0.00001;

        // Calcoliamo lo spazio utile sottraendo il margine (es. 0.15 = 15% di bordi)
        double targetWidth = canvasSize.x * (1.0 - paddingPct);
        double targetHeight = canvasSize.y * (1.0 - paddingPct);

        // Calcolo dello zoom per asse X e Y
        double zoomX = std::log2(targetWidth / (dx * 256.0));
        double zoomY = std::log2(targetHeight / (dy * 256.0));

        // Prendiamo lo zoom più stringente per far stare tutto e limitiamolo ai livelli consentiti
        outZoom = (float)std::min(zoomX, zoomY);
        outZoom = std::max(2.0f, std::min(21.0f, outZoom));
    }
}

