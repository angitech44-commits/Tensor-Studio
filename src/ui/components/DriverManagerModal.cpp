#include "DriverManagerModal.hpp"
#include "../../io/DriverSerializer.hpp"
#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <cctype>

#ifndef PROJECT_ROOT_DIR
#define PROJECT_ROOT_DIR "."
#endif

DriverManagerModal::DriverManagerModal() {}

void DriverManagerModal::RefreshDriverList() {
    savedDrivers = DriverSerializer::GetAvailableDrivers();
}

std::string DriverManagerModal::GetUniqueDriverName(const std::string& baseName) {
    std::string newName = baseName;
    int counter = 1;
    while (std::find(savedDrivers.begin(), savedDrivers.end(), newName) != savedDrivers.end()) {
        newName = baseName + "_" + std::to_string(counter);
        counter++;
    }
    return newName;
}

void DriverManagerModal::Open() {
    shouldOpen = true; 
    isDriverActive = false;
    currentFileName = "";
    searchBuffer[0] = '\0';
    RefreshDriverList();
}

void DriverManagerModal::Render() {
    if (shouldOpen) {
        ImGui::OpenPopup("Driver Manager");
        shouldOpen = false;
        isOpen = true;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 center = viewport->GetCenter();
    float scale = ImGui::GetIO().FontGlobalScale;
    
    ImVec2 modalSize = ImVec2(800.0f * scale, 500.0f * scale); // Più piccolo del Track Manager

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(modalSize, ImGuiCond_Always);

    bool keepOpen = true; 
    bool isModalRendering = ImGui::BeginPopupModal("Driver Manager", &keepOpen, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        
    if (!keepOpen) {
        if (isDriverActive && isDriverModified) {
            ImGui::OpenPopup("Driver Manager"); 
            showUnsavedChangesPopup = true;
            pendingDriverLoad = "##CLOSE_MANAGER##";
        } else {
            isOpen = false;
        }
    }

    if (isModalRendering) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float leftColWidth = 240.0f * scale;
        float rightColWidth = avail.x - leftColWidth - (8.0f * scale);

        if (ImGui::BeginChild("DriverLibrary", ImVec2(leftColWidth, 0), true)) RenderLibraryColumn();
        ImGui::EndChild();
        
        ImGui::SameLine();

        if (isDriverActive) {
            if (ImGui::BeginChild("DriverProperties", ImVec2(rightColWidth, 0), true)) RenderPropertiesColumn();
            ImGui::EndChild();
        } else {
            if (ImGui::BeginChild("EmptyState", ImVec2(0, 0), true)) {
                ImVec2 emptyAvail = ImGui::GetContentRegionAvail();
                const char* msg = "Select a driver or create a new one";
                ImVec2 textSize = ImGui::CalcTextSize(msg);
                ImGui::SetCursorPos(ImVec2((emptyAvail.x - textSize.x) * 0.5f, (emptyAvail.y - textSize.y) * 0.5f));
                ImGui::TextDisabled("%s", msg);
            }
            ImGui::EndChild();
        }

        if (!isOpen) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void DriverManagerModal::RenderLibraryColumn() {
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "DRIVERS");
    ImGui::Separator();
    
    ImGui::InputTextWithHint("##SearchDriver", "Search...", searchBuffer, sizeof(searchBuffer));
    ImGui::Dummy(ImVec2(0, 2.0f));
    ImGui::Separator();

    std::string searchStr(searchBuffer);
    std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

    float buttonHeight = 35.0f * ImGui::GetIO().FontGlobalScale;
    if (ImGui::BeginChild("DriverListScroll", ImVec2(0, ImGui::GetContentRegionAvail().y - buttonHeight))) {
        for (const auto& dName : savedDrivers) {
            if (!searchStr.empty()) {
                std::string dNameLower = dName;
                std::transform(dNameLower.begin(), dNameLower.end(), dNameLower.begin(), ::tolower);
                if (dNameLower.find(searchStr) == std::string::npos) continue;
            }

            bool isSelected = (currentFileName == dName);
            if (ImGui::Selectable(dName.c_str(), isSelected)) {
                if (isDriverActive && isDriverModified && currentFileName != dName) {
                    showUnsavedChangesPopup = true;
                    pendingDriverLoad = dName; 
                } else if (currentFileName != dName) { 
                    currentFileName = dName;
                    isDriverActive = true;
                    isDriverModified = false;
                    std::string path = std::string(PROJECT_ROOT_DIR) + "/assets/drivers/" + dName + ".json";
                    DriverSerializer::LoadDriver(path, activeDriver);
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (ImGui::Button("Create New", ImVec2(-1, 0))) {
        if (isDriverActive && isDriverModified) {
            showUnsavedChangesPopup = true;
            pendingDriverLoad = "##NEW_DRIVER##";
        } else {
            activeDriver = DriverProfile();
            activeDriver.name = GetUniqueDriverName("NewDriver");
            currentFileName = "";
            isDriverActive = true;
            isDriverModified = false;
        }
    }
}

void DriverManagerModal::RenderPropertiesColumn() {
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "DRIVER PROFILE");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 5.0f));

    char buf[64];
    memset(buf, 0, sizeof(buf));
    strncpy(buf, activeDriver.name.c_str(), sizeof(buf) - 1);
    
    ImGui::TextDisabled("Name:");
    if (ImGui::InputText("##DriverName", buf, sizeof(buf))){
        activeDriver.name = buf;
        isDriverModified = true;
    }

    auto saveCurrentDriver = [&]() {
        if (!currentFileName.empty() && currentFileName != activeDriver.name) {
            std::string oldPath = std::string(PROJECT_ROOT_DIR) + "/assets/drivers/" + currentFileName + ".json";
            if (std::filesystem::exists(oldPath)) std::filesystem::remove(oldPath);
        }
        std::string path = std::string(PROJECT_ROOT_DIR) + "/assets/drivers/" + activeDriver.name + ".json";
        DriverSerializer::SaveDriver(activeDriver, path);
        currentFileName = activeDriver.name;
        isDriverModified = false;
        RefreshDriverList();
    };

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        if (!activeDriver.name.empty()) {
            bool nameExists = std::find(savedDrivers.begin(), savedDrivers.end(), activeDriver.name) != savedDrivers.end();
            if (nameExists && activeDriver.name != currentFileName) showOverwritePopup = true;
            else saveCurrentDriver();
        }
    }

    ImGui::Dummy(ImVec2(0, ImGui::GetContentRegionAvail().y - 80.0f * ImGui::GetIO().FontGlobalScale));
    ImGui::Separator();
    
    if (ImGui::Button("Save to Library", ImVec2(-1, 0))) {
        if (!activeDriver.name.empty()) {
            bool nameExists = std::find(savedDrivers.begin(), savedDrivers.end(), activeDriver.name) != savedDrivers.end();
            if (nameExists && activeDriver.name != currentFileName) showOverwritePopup = true;
            else saveCurrentDriver();
        }
    }

    ImGui::Dummy(ImVec2(0, 5.0f));
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button("Delete Driver from Library", ImVec2(-1, 0))) {
        if (!currentFileName.empty()) showDeletePopup = true;
    }
    ImGui::PopStyleColor(3);

    // Popups per salvataggio e cancellazione
    if (showOverwritePopup) ImGui::OpenPopup("Overwrite Driver Warning");
    if (ImGui::BeginPopupModal("Overwrite Driver Warning", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("A driver named '%s' already exists.\nDo you want to overwrite it?", activeDriver.name.c_str());
        ImGui::Separator();
        if (ImGui::Button("Yes, Overwrite", ImVec2(120, 0))) {
            saveCurrentDriver();
            showOverwritePopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { showOverwritePopup = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    if (showDeletePopup) ImGui::OpenPopup("Delete Driver Warning");
    if (ImGui::BeginPopupModal("Delete Driver Warning", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to permanently delete '%s'?\nThis action cannot be undone.", currentFileName.c_str());
        ImGui::Separator();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
            std::string path = std::string(PROJECT_ROOT_DIR) + "/assets/drivers/" + currentFileName + ".json";
            if (std::filesystem::exists(path)) std::filesystem::remove(path);
            
            activeDriver = DriverProfile();
            currentFileName = "";
            isDriverActive = false;
            RefreshDriverList();
            showDeletePopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { showDeletePopup = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    if (showUnsavedChangesPopup) ImGui::OpenPopup("Unsaved Driver Changes");
    if (ImGui::BeginPopupModal("Unsaved Driver Changes", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes.\nIf you continue, these changes will be lost.");
        ImGui::Separator();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Discard Changes", ImVec2(140, 0))) {
            if (pendingDriverLoad == "##NEW_DRIVER##") {
                activeDriver = DriverProfile();
                activeDriver.name = GetUniqueDriverName("NewDriver");
                currentFileName = "";
                isDriverActive = true;
                isDriverModified = false;
            } else if (pendingDriverLoad == "##CLOSE_MANAGER##") {
                if (!currentFileName.empty()) {
                    std::string path = std::string(PROJECT_ROOT_DIR) + "/assets/drivers/" + currentFileName + ".json";
                    DriverSerializer::LoadDriver(path, activeDriver);
                } else {
                    activeDriver = DriverProfile(); 
                }
                isDriverModified = false;
                isOpen = false; 
            } else {
                currentFileName = pendingDriverLoad;
                isDriverActive = true;
                isDriverModified = false;
                std::string path = std::string(PROJECT_ROOT_DIR) + "/assets/drivers/" + pendingDriverLoad + ".json";
                DriverSerializer::LoadDriver(path, activeDriver);
            }
            showUnsavedChangesPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { 
            showUnsavedChangesPopup = false; 
            ImGui::CloseCurrentPopup(); 
        }
        ImGui::EndPopup();
    }
}