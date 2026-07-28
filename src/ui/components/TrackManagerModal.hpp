#pragma once
#include "../../core/Track.hpp"
#include "TrackMapView.hpp"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <vector>
#include <string>
#include <mutex>
#include <memory>

enum class TrackEditorMode {
    ViewOnly,
    DrawLeftBound,
    DrawRightBound,
    PlaceFinishLine,
    PlaceSector,
    DeletePoint
};

class TrackManagerModal {
private:
    bool isOpen = false;
    bool shouldOpen = false;
    bool isTrackActive = false;
    
    TrackEditorMode currentMode = TrackEditorMode::ViewOnly;
    Track activeTrack;
    std::string currentFileName;
    TrackMapView mapView;

    std::vector<std::string> savedTracks;

    bool isPlacingFence = false;
    double tempFenceLat = 0.0;
    double tempFenceLon = 0.0;

    bool showOverwritePopup = false;
    bool showDeletePopup = false;

    enum class DraggedPointType { None, LeftBound, RightBound, FenceP1, FenceP2 };
    DraggedPointType draggedPointType = DraggedPointType::None;
    int draggedPointIndex = -1;

    std::unique_ptr<std::mutex> dialogMutex = std::make_unique<std::mutex>();
    std::string pendingImportPath;
    std::string pendingExportPath;
    bool triggerImport = false;
    bool triggerExport = false;

    char searchBuffer[128] = "";
    std::vector<Track> trackHistory;

    // --- NUOVI HELPER PER SNELLIRE IL CODICE ---
    bool needsMapCentering = false;
    std::string GetUniqueTrackName(const std::string& baseName);

    void SaveStateForUndo();
    void PerformUndo();

    void RefreshTrackList();
    void RenderLibraryColumn();
    void RenderCanvasColumn(SDL_Renderer* renderer);
    void RenderToolsColumn();

    void HandleMapInput(ImVec2 canvasCenter, bool isMapHovered);
    void RenderTrackOverlays(ImDrawList* drawList, ImVec2 canvasCenter, bool isMapHovered);

public:
    TrackManagerModal();
    void Open();
    void Render(SDL_Renderer* renderer);
};