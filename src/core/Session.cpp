#include "Session.hpp"
#include <cstdio>

Session::Session() {
    snprintf(sessionName, sizeof(sessionName), "Track_Day_01");
    snprintf(sessionDate, sizeof(sessionDate), "2026-07-19");
    selectedTrack.name = "Local Circuit";
}