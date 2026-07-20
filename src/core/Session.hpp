#pragma once
#include <string>
#include <vector>
#include "DriverEntry.hpp"
#include "TrackConfig.hpp"

struct Session {
    char sessionName[128];
    char sessionDate[64];
    TrackConfig selectedTrack;
    std::vector<DriverEntry> drivers;

    Session();
};