#pragma once
#include "../../core/DriverProfile.hpp"
#include <SDL3/SDL.h>
#include <string>
#include <vector>

class DriverManagerModal {
private:
    bool isOpen = false;
    bool shouldOpen = false;
    bool isDriverActive = false;
    
    DriverProfile activeDriver;
    std::string currentFileName;
    std::vector<std::string> savedDrivers;

    bool showOverwritePopup = false;
    bool showDeletePopup = false;
    bool isDriverModified = false;
    bool showUnsavedChangesPopup = false;
    std::string pendingDriverLoad;

    char searchBuffer[128] = "";

    std::string GetUniqueDriverName(const std::string& baseName);
    void RefreshDriverList();
    void RenderLibraryColumn();
    void RenderPropertiesColumn();

public:
    DriverManagerModal();
    void Open();
    void Render();
};