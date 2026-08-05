#pragma once

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace hephaiston {

class EditorContext;

enum class ViewMode {
    Mode2D,
    Mode3D,
};

enum class TrackpadZoomGestureMode {
    TwoFingerScroll,
    Pinch,
};

enum class SceneObjectKind {
    Unknown,
    Project,
    Georeference,
    Site,
    Boundary,
    Regulation,
    Envelope,
    Building,
    Floor,
    Roof,
    GeneratedModel,
    PluginObject,
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

struct EditorMenuVisibility {
    bool showFile = true;
    bool showEdit = true;
    bool showView = true;
    bool showPlugins = true;
    bool showWindow = true;
    bool showHelp = true;
    bool showFpsControl = true;
};

struct ViewportRenderSettings {
    bool showHorizontalGrid = true;
    bool showOriginAxes = true;
    bool showScaleBar = true;
    // Core's upper-left 2D/3D toggle. Plugins that own the view mode can
    // disable this and expose their own mode controls instead.
    bool showViewModeToggle = true;
};

struct ViewportNavigationSettings {
    // UI values are calibrated so 1.00x equals the former 0.75x response.
    // Always use the effective accessors when applying an input delta.
    static constexpr float kResponseBaseline = 0.75f;
    float zoomSensitivity = 1.0f;
    float moveSensitivity = 1.0f;
    TrackpadZoomGestureMode trackpadZoomGestureMode = TrackpadZoomGestureMode::TwoFingerScroll;

    [[nodiscard]] float effectiveZoomSensitivity() const noexcept {
        return zoomSensitivity * kResponseBaseline;
    }
    [[nodiscard]] float effectiveMoveSensitivity() const noexcept {
        return moveSensitivity * kResponseBaseline;
    }
    // Orbiting needs less angular movement at close range. This keeps small
    // objects controllable without making distant CAD views sluggish.
    [[nodiscard]] float effectiveOrbitZoomSensitivity(float orbitDistanceMeters) const noexcept {
        return effectiveZoomSensitivity() * orbitDistanceResponse(orbitDistanceMeters);
    }
    [[nodiscard]] float effectiveOrbitMoveSensitivity(float orbitDistanceMeters) const noexcept {
        return effectiveMoveSensitivity() * orbitDistanceResponse(orbitDistanceMeters);
    }

private:
    [[nodiscard]] static float orbitDistanceResponse(float orbitDistanceMeters) noexcept {
        constexpr float referenceDistanceMeters = 42.0f;
        const float ratio = std::max(0.001f, orbitDistanceMeters) / referenceDistanceMeters;
        return std::clamp(std::sqrt(ratio), 0.025f, 1.50f);
    }
};

// Lightweight command descriptor for simple add-ons. For richer commands with
// context-aware enable/execute logic, prefer IEditorCommand.
struct EditorCommand {
    std::string id;
    std::string displayName;
    std::function<void()> execute;
};

struct EditorMenuItem {
    std::string id;
    std::string menuName;
    std::string label;
    std::string shortcut;
    bool selected = false;
    bool enabled = true;
    std::function<void()> execute;
};

struct StatusBarItem {
    std::string id;
    std::string text;
    bool visible = true;
};

struct EditorHierarchyItem {
    std::string id;
    std::string displayName;
    std::vector<EditorHierarchyItem> children;
};

struct SceneObject {
    std::string id;
    std::string displayName;
    SceneObjectKind kind = SceneObjectKind::Unknown;
    bool visible = true;
    std::vector<SceneObject> children;
};

struct SelectionItem {
    std::string id;
    std::string displayName;
    std::string type;
};

struct ViewportLine {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float z1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    float z2 = 0.0f;
    ImVec4 color {1.0f, 1.0f, 1.0f, 1.0f};
};

struct ViewportPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float sizePixels = 4.0f;
    ImVec4 color {1.0f, 1.0f, 1.0f, 1.0f};
};

struct ViewportTriangle {
    ImVec4 color {1.0f, 1.0f, 1.0f, 1.0f};
    float vertices[9] {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
};

struct ViewportTexturedVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
};

// Generic textured geometry extension point. Core owns OpenGL texture upload;
// modules/plugins only supply CPU vertices, indices and an asset path.
struct ViewportTexturedMesh {
    std::string id;
    std::string texturePath;
    std::vector<ViewportTexturedVertex> vertices;
    std::vector<unsigned int> indices;
    // Useful for raster label overlays: light map backgrounds become
    // transparent while dark place names and line work remain visible.
    bool makeLightPixelsTransparent = false;
};

class IMainMenuPanel {
public:
    virtual ~IMainMenuPanel() = default;
    virtual void draw() = 0;
};

class IHierarchyPanel {
public:
    virtual ~IHierarchyPanel() = default;
    virtual void draw() = 0;
};

class IFloatingWindow {
public:
    virtual ~IFloatingWindow() = default;
    virtual void draw() = 0;
};

class IStatusBarWidget {
public:
    virtual ~IStatusBarWidget() = default;
    virtual void draw() = 0;
};

class IMenuBarContributor {
public:
    virtual ~IMenuBarContributor() = default;
    virtual void draw(EditorContext& context) = 0;
};

struct RegisteredMainMenuPanel {
    std::string id;
    std::string displayName;
    std::unique_ptr<IMainMenuPanel> panel;
};

struct RegisteredHierarchyPanel {
    std::string id;
    std::string displayName;
    std::unique_ptr<IHierarchyPanel> panel;
};

struct RegisteredFloatingWindow {
    std::string id;
    std::string displayName;
    bool open = false;
    std::unique_ptr<IFloatingWindow> window;
};

struct RegisteredStatusBarWidget {
    std::string id;
    std::string displayName;
    bool visible = true;
    std::unique_ptr<IStatusBarWidget> widget;
};

struct RegisteredMenuBarContributor {
    std::string id;
    std::unique_ptr<IMenuBarContributor> contributor;
};

// Compatibility interfaces. New plugin code should prefer the loose draw-only
// interfaces above and pass metadata through EditorRegistry registration calls.
class IEditorPanel : public IMainMenuPanel {
public:
    virtual ~IEditorPanel() = default;
    virtual const char* id() const = 0;
    virtual const char* displayName() const = 0;
};

class IEditorWindow : public IFloatingWindow {
public:
    virtual ~IEditorWindow() = default;
    virtual const char* id() const = 0;
    virtual const char* displayName() const = 0;
    virtual bool& open() = 0;
};

class IViewportOverlay {
public:
    virtual ~IViewportOverlay() = default;
    virtual const char* id() const = 0;
    virtual void draw(const ViewportVisibleRect& visibleRect, ViewportStatus& status, ViewMode& viewMode) = 0;
};

class IHierarchyProvider {
public:
    virtual ~IHierarchyProvider() = default;
    virtual const char* id() const = 0;
    virtual void collectHierarchy(std::vector<EditorHierarchyItem>& outItems) = 0;
};

class ISceneProvider {
public:
    virtual ~ISceneProvider() = default;
    virtual const char* id() const = 0;
    virtual void collectSceneObjects(std::vector<SceneObject>& outObjects) = 0;
};

class IViewportSceneLayer {
public:
    virtual ~IViewportSceneLayer() = default;
    virtual const char* id() const = 0;
    virtual const char* displayName() const = 0;
    virtual bool visible() const { return true; }
    virtual void collectViewportLines(std::vector<ViewportLine>& outLines) = 0;
    virtual void collectViewportPoints(std::vector<ViewportPoint>&) {}
    virtual void collectViewportTexturedMeshes(std::vector<ViewportTexturedMesh>&) {}
};

class IViewportTool {
public:
    virtual ~IViewportTool() = default;
    virtual const char* id() const = 0;
    virtual const char* displayName() const = 0;
    virtual void onActivated(EditorContext&) {}
    virtual void onDeactivated(EditorContext&) {}
    virtual void drawToolbar(EditorContext&) {}
    virtual bool handlesViewportNavigation(EditorContext&) const { return false; }
    // Return true while a modal tool owns viewport input (for example polygon
    // vertex capture). Core will not pan, zoom, orbit, select, or operate the
    // default view gizmo during that period.
    virtual bool blocksDefaultViewportNavigation(EditorContext&) const { return false; }
    virtual void onViewportInput(EditorContext&) {}
};

class IEditorCommand {
public:
    virtual ~IEditorCommand() = default;
    virtual const char* id() const = 0;
    virtual const char* displayName() const = 0;
    virtual bool canExecute(EditorContext&) const { return true; }
    virtual void execute(EditorContext&) = 0;
};

class IPropertiesPanel {
public:
    virtual ~IPropertiesPanel() = default;
    virtual const char* id() const = 0;
    virtual bool canInspect(const SelectionItem& item) const = 0;
    virtual void draw(EditorContext& context, const SelectionItem& item) = 0;
};

class IContextMenuProvider {
public:
    virtual ~IContextMenuProvider() = default;
    virtual const char* id() const = 0;
    virtual void drawHierarchyContextMenu(EditorContext&, const std::string&) {}
    virtual void drawViewportContextMenu(EditorContext&) {}
};

} // namespace hephaiston
