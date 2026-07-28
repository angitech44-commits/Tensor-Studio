#pragma once
#include "../core/Track.hpp"
#include <string>
#include <vector>

class TrackSerializer {
public:
    static bool SaveTrack(const Track& track, const std::string& filepath);
    
    static bool LoadTrack(const std::string& filepath, Track& outTrack);
    
    static std::vector<std::string> GetAvailableTracks(const std::string& directory = "assets/tracks/");
};