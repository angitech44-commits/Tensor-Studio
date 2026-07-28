#pragma once
#include <string>
#include <vector>
#include "DriverEntry.hpp"
#include "Track.hpp"

struct Session {
    char sessionName[128];
    char sessionDate[64];
    Track selectedTrack;
    std::vector<DriverEntry> drivers;

    Session();
};