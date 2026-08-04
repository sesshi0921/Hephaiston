#pragma once

#include <imgui.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace hephaiston {

enum class ViewMode {
    Mode2D,
    Mode3D,
};

struct ViewportStatus {
    // Shared viewport readout.
    double zoom = 16.4;                 // 2D: pixels per meter / 3D: derived display value.
    double metersPerPixel = 0.061;
    double gridSizeMeters = 1.0;
    int scaleDenominator = 500;
    std::string coordinateReferenceSystem = "EPSG:6677";
    ImVec2 cursorMeters {0.0f, 0.0f};

    // 2D CAD camera. panMeters is the world coordinate at the center of the viewport.
    ImVec2 panMeters {0.0f, 0.0f};

    // 3D orbit camera. Z is up. Target is the orbit center in world meters.
    float orbitYawDegrees = 45.0f;
    float orbitPitchDegrees = 35.0f;
    float orbitDistanceMeters = 42.0f;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 2.0f;
};

struct ViewportVisibleRect {
    ImVec2 min {0.0f, 0.0f};
    ImVec2 max {0.0f, 0.0f};
};

struct EditorLayoutState {
    bool leftPanelCollapsed = false;
    bool rightPanelCollapsed = false;
    float leftPanelWidth = 300.0f;
    float rightPanelWidth = 320.0f;
    bool showStatusBar = true;
};

struct ViewportInputState {
    bool hovered = false;
    bool clicked = false;
    bool dragging = false;
    bool panning = false;
    bool rotating = false;
    float wheel = 0.0f;
    ImVec2 mousePos {0.0f, 0.0f};
    ImVec2 dragDelta {0.0f, 0.0f};
};

struct EditorCommand {
    std::string id;
    std::string displayName;
    std::function<void()> execute;
};

class IEditorPanel {
public:
    virtual ~IEditorPanel() = default;
    virtual const char* id() const = 0;
    virtual const char* displayName() const = 0;
    virtual void draw() = 0;
};

class IEditorWindow {
public:
    virtual ~IEditorWindow() = default;
    virtual const char* id() const = 0;
    virtual const char* displayName() const = 0;
    virtual bool& open() = 0;
    virtual void draw() = 0;
};

class IViewportOverlay {
public:
    virtual ~IViewportOverlay() = default;
    virtual const char* id() const = 0;
    virtual void draw(const ViewportVisibleRect& visibleRect, ViewportStatus& status, ViewMode& viewMode) = 0;
};

} // namespace hephaiston
