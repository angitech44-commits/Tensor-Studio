#pragma once
#include <string>
#include <vector>

enum class TrackFenceType {
    FinishLine,
    Sector
};

struct GeoPoint {
    double lat = 0.0;
    double lon = 0.0;
};

struct TrackFence {
    std::string name;
    TrackFenceType type = TrackFenceType::Sector;
    GeoPoint p1;
    GeoPoint p2;
    float directionHeading = 0.0f;
};

struct TrackLimits {
    std::vector<GeoPoint> leftBound;
    std::vector<GeoPoint> rightBound;
};

struct Track {
    
    std::string name = ""; 
    
    float lengthMeters = 0.0f;
    
    
    std::vector<TrackFence> fences; 
    
    
    TrackLimits limits;
};