#pragma once
#include "../../core/Session.hpp"
#include <vector>
#include <string>

class SessionSetupModal {
private:
    bool showValidationError = false;
    bool isTrackEditMode = false;
    char newDriverInput[128] = "";
    char trackNameBuffer[128] = "";
    std::string originalTrackName = "";
    std::vector<std::string> availableTracks = {"Local Circuit", "Monza Circuit", "Vallelunga", "Imola"};

public:
    void Render(Session& session, bool& isProjectOpen, int& activeTab);
};