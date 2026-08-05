#include "hephaiston/EditorShell.h"

#include "hephaiston/EditorContext.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

namespace hephaiston {
namespace {
constexpr float kCollapsedPanelWidth = 42.0f;
constexpr float kSplitterWidth = 8.0f;
constexpr float kStatusBarHeight = 24.0f;
constexpr float kMinPanelWidth = 220.0f;
constexpr float kMaxPanelWidth = 520.0f;
constexpr ImVec2 kScaleOverlaySize {164.0f, 30.0f};
constexpr ImVec2 kViewGizmoOverlaySize {108.0f, 108.0f};
constexpr ImVec2 kViewModeToggleOverlaySize {112.0f, 32.0f};
constexpr ImVec2 kPanelToggleButtonSize {18.0f, 18.0f};
constexpr float kPanelToggleEdgePadding = 8.0f;

std::filesystem::path settingsPath() {
    return std::filesystem::current_path() / "hephaiston_layout.ini";
}

bool drawPanelToggleButton(const char* id, bool pointsRight) {
    const bool pressed = ImGui::Button(id, kPanelToggleButtonSize);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    const float halfW = 4.2f;
    const float halfH = 5.2f;
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
    if (pointsRight) {
        ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(center.x + halfW, center.y), ImVec2(center.x - halfW, center.y - halfH), ImVec2(center.x - halfW, center.y + halfH), color);
    } else {
        ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(center.x - halfW, center.y), ImVec2(center.x + halfW, center.y - halfH), ImVec2(center.x + halfW, center.y + halfH), color);
    }
    return pressed;
}

bool drawPanelToggleButtonAt(const char* id, ImVec2 screenPos, bool pointsRight) {
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 min = screenPos;
    const ImVec2 max(screenPos.x + kPanelToggleButtonSize.x, screenPos.y + kPanelToggleButtonSize.y);
    const bool hovered = io.MousePos.x >= min.x && io.MousePos.x <= max.x && io.MousePos.y >= min.y && io.MousePos.y <= max.y;
    const bool pressed = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImU32 bg = ImGui::GetColorU32(pressed ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
    drawList->AddRectFilled(min, max, bg, 4.0f);
    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    const float halfW = 4.2f;
    const float halfH = 5.2f;
    if (pointsRight) {
        drawList->AddTriangleFilled(ImVec2(center.x + halfW, center.y), ImVec2(center.x - halfW, center.y - halfH), ImVec2(center.x - halfW, center.y + halfH), color);
    } else {
        drawList->AddTriangleFilled(ImVec2(center.x - halfW, center.y), ImVec2(center.x + halfW, center.y - halfH), ImVec2(center.x + halfW, center.y + halfH), color);
    }
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    (void)id;
    return pressed;
}

class GenericEditorWindow final : public IEditorWindow {
public:
    GenericEditorWindow(std::string id, std::string title) : id_(std::move(id)), title_(std::move(title)) {}

    const char* id() const override { return id_.c_str(); }
    const char* displayName() const override { return title_.c_str(); }
    bool& open() override { return open_; }

    void draw() override {
        ImGui::TextUnformatted(title_.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("This is a plugin-ready floating editor window. The current Core shell keeps only dummy settings here.");
        if (title_ == "Project Settings") {
            static int units = 0;
            ImGui::Combo("Length Unit", &units, "meters\0millimeters\0feet\0");
            ImGui::InputText("Project CRS", crs_, sizeof(crs_));
        } else if (title_ == "Plugin Manager") {
            ImGui::TextDisabled("DLL/dylib plugins are discovered from plugins/ and build/plugins/.");
            ImGui::TextWrapped("Use the Plugins menu to select a registered plugin panel.");
        } else if (title_ == "Console") {
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "[Info] Hephaiston Core shell started.");
            ImGui::TextColored(ImVec4(0.45f, 0.65f, 1.0f, 1.0f), "[Viewport] FBO renderer active.");
        } else if (title_ == "Debug Information") {
            ImGuiIO& io = ImGui::GetIO();
            ImGui::Text("FPS: %.1f", io.Framerate);
            ImGui::Text("Display: %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
        } else if (title_ == "About") {
            ImGui::TextWrapped("Hephaiston is an extensible architectural design editor prototype built with C++20, Dear ImGui, GLFW and OpenGL.");
        } else {
            ImGui::TextDisabled("Placeholder content for future add-ons.");
        }
    }

private:
    std::string id_;
    std::string title_;
    bool open_ = false;
    char crs_[32] = "EPSG:6677";
};

class ViewportStatusOverlay final : public IViewportOverlay {
public:
    const char* id() const override { return "overlay.viewport_status"; }

    void draw(const ViewportVisibleRect& rect, ViewportStatus& status, ViewMode& mode) override {
        const ImVec2 pos(rect.min.x + 2.0f, rect.max.y - kScaleOverlaySize.y - 2.0f);
        const double scaleMeters = niceDistanceMeters(status.metersPerPixel * 82.0);
        const std::string label = formatDistance(scaleMeters);

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(kScaleOverlaySize, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground;
        ImGui::Begin("##ViewportScaleOverlay", nullptr, flags);

        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##ViewportScaleBarHitArea", kScaleOverlaySize);
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            detailsPinned_ = true;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + kScaleOverlaySize.x, canvasPos.y + kScaleOverlaySize.y), IM_COL32(246, 248, 250, 132), 3.0f);
        const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
        const ImVec2 labelPos(canvasPos.x + 6.0f, canvasPos.y + 7.0f);
        const float barStartX = labelPos.x + labelSize.x + 8.0f;
        const float maxBarWidth = std::max(46.0f, canvasPos.x + kScaleOverlaySize.x - barStartX - 8.0f);
        const float barWidth = static_cast<float>(std::clamp(scaleMeters / std::max(0.001, status.metersPerPixel), 42.0, static_cast<double>(maxBarWidth)));
        const ImVec2 barStart(barStartX, canvasPos.y + 20.0f);
        const ImVec2 barEnd(barStart.x + barWidth, barStart.y);
        const ImU32 ink = IM_COL32(24, 24, 24, 255);
        drawList->AddText(labelPos, ink, label.c_str());
        drawList->AddLine(barStart, barEnd, ink, 3.0f);
        drawList->AddLine(ImVec2(barStart.x, barStart.y), ImVec2(barStart.x, barStart.y - 7.0f), ink, 3.0f);
        drawList->AddLine(ImVec2(barEnd.x, barEnd.y), ImVec2(barEnd.x, barEnd.y - 7.0f), ink, 3.0f);
        ImGui::End();

        if (hovered || detailsPinned_) {
            drawDetailsPopup(rect, pos, status, mode, label, barWidth, detailsPinned_);
        }
    }

private:
    static double niceDistanceMeters(double meters) {
        constexpr double minMeters = 0.2;
        constexpr double maxMeters = 10000000.0;
        meters = std::clamp(meters, minMeters, maxMeters);
        const double power = std::pow(10.0, std::floor(std::log10(std::max(0.001, meters))));
        const double normalized = meters / power;
        double nice = 1.0;
        if (normalized >= 5.0) nice = 5.0;
        else if (normalized >= 2.0) nice = 2.0;
        return std::clamp(nice * power, minMeters, maxMeters);
    }

    static std::string formatDistance(double meters) {
        char buffer[32] = {};
        if (meters >= 1000.0) {
            const double km = meters / 1000.0;
            if (km >= 10.0 || std::abs(km - std::round(km)) < 0.05) std::snprintf(buffer, sizeof(buffer), "%.0f km", km);
            else std::snprintf(buffer, sizeof(buffer), "%.1f km", km);
        } else if (meters >= 10.0 || std::abs(meters - std::round(meters)) < 0.05) {
            std::snprintf(buffer, sizeof(buffer), "%.0f m", meters);
        } else {
            std::snprintf(buffer, sizeof(buffer), "%.1f m", meters);
        }
        return buffer;
    }

    void drawDetailsPopup(const ViewportVisibleRect& rect,
                          ImVec2 scalePos,
                          const ViewportStatus& status,
                          ViewMode mode,
                          const std::string& scaleLabel,
                          float barWidth,
                          bool pinned) {
        constexpr ImVec2 popupSize {286.0f, 172.0f};
        const float popupY = std::max(rect.min.y + 8.0f, scalePos.y - popupSize.y - 6.0f);
        ImGui::SetNextWindowPos(ImVec2(scalePos.x, popupY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(popupSize, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.88f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
        ImGui::Begin("##ViewportScaleDetailsPopup", nullptr, flags);
        ImGui::TextUnformatted("Viewport Info");
        if (pinned) {
            ImGui::SameLine(ImGui::GetWindowWidth() - 30.0f);
            if (ImGui::SmallButton("×")) detailsPinned_ = false;
        }
        ImGui::Separator();
        ImGui::Text("Scale bar: %s", scaleLabel.c_str());
        ImGui::Text("Bar pixels: %.0f px", barWidth);
        ImGui::Text("Scale: 1:%d", status.scaleDenominator);
        ImGui::Text("Grid: %.2f m", status.gridSizeMeters);
        ImGui::Text("Zoom: %.2f", status.zoom);
        ImGui::Text("CRS: %s", status.coordinateReferenceSystem.c_str());
        ImGui::Text("Cursor: X %.2f m / Y %.2f m", status.cursorMeters.x, status.cursorMeters.y);
        if (mode == ViewMode::Mode3D) ImGui::Text("3D: yaw %.0f / pitch %.0f / dist %.1fm", status.orbitYawDegrees, status.orbitPitchDegrees, status.orbitDistanceMeters);
        if (!pinned) ImGui::TextDisabled("Click scale bar to pin");
        ImGui::End();
    }

    bool detailsPinned_ = false;
};

class ViewModeToggleOverlay final : public IViewportOverlay {
public:
    const char* id() const override { return "overlay.view_mode_toggle"; }

    void draw(const ViewportVisibleRect& rect, ViewportStatus&, ViewMode& mode) override {
        const ImVec2 pos(rect.min.x + 8.0f, rect.min.y + 8.0f);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(kViewModeToggleOverlaySize, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground;
        ImGui::Begin("##ViewModeToggleOverlay", nullptr, flags);
        auto drawModeButton = [&mode](const char* label, ViewMode buttonMode) {
            const bool active = mode == buttonMode;
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.54f, 0.82f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.38f, 0.64f, 0.92f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.70f, 1.00f, 1.0f));
            }
            if (ImGui::Button(label, ImVec2(48.0f, 0.0f))) mode = buttonMode;
            if (active) ImGui::PopStyleColor(3);
        };
        drawModeButton("2D", ViewMode::Mode2D);
        ImGui::SameLine();
        drawModeButton("3D", ViewMode::Mode3D);
        ImGui::End();
    }
};

class ViewGizmoOverlay final : public IViewportOverlay {
public:
    explicit ViewGizmoOverlay(EditorRegistry& registry) : registry_(registry) {}
    const char* id() const override { return "overlay.view_gizmo"; }

    void draw(const ViewportVisibleRect& rect, ViewportStatus& status, ViewMode& mode) override {
        const ImVec2 size = kViewGizmoOverlaySize;
        const ImVec2 pos(rect.max.x - size.x - 6.0f, rect.max.y - size.y - 6.0f);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground;
        ImGui::Begin("##ViewGizmoOverlay", nullptr, flags);

        const ImVec2 canvasSize(94.0f, 94.0f);
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##ViewportGizmoCanvas", canvasSize);
        const bool canvasHovered = ImGui::IsItemHovered();
        const bool canvasActive = ImGui::IsItemActive();
        if (mode == ViewMode::Mode3D && canvasActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const ImVec2 delta = ImGui::GetIO().MouseDelta;
            // Keep the globe/CAD drag behaviour consistent with 2D map pan:
            // dragging content right/up moves the view west/south.
            const float sensitivity = registry_.viewportNavigationSettings().effectiveOrbitMoveSensitivity(status.orbitDistanceMeters);
            status.orbitYawDegrees -= delta.x * 0.35f * sensitivity;
            status.orbitPitchDegrees = std::clamp(status.orbitPitchDegrees + delta.y * 0.28f * sensitivity, -82.0f, 82.0f);
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(12, 16, 21, canvasHovered ? 118 : 78), 10.0f);
        const ImVec2 center(canvasPos.x + canvasSize.x * 0.50f, canvasPos.y + canvasSize.y * 0.56f);
        drawList->AddCircleFilled(center, 3.2f, IM_COL32(230, 235, 242, 220));

        auto drawArrow = [drawList](ImVec2 from, ImVec2 to, ImU32 color, const char* label) {
            drawList->AddLine(from, to, color, 2.0f);
            const float angle = std::atan2(to.y - from.y, to.x - from.x);
            const float headLength = 6.0f;
            const float headAngle = 0.55f;
            const ImVec2 p1(to.x - std::cos(angle - headAngle) * headLength, to.y - std::sin(angle - headAngle) * headLength);
            const ImVec2 p2(to.x - std::cos(angle + headAngle) * headLength, to.y - std::sin(angle + headAngle) * headLength);
            drawList->AddTriangleFilled(to, p1, p2, color);
            drawList->AddText(ImVec2(to.x + 3.0f, to.y - 8.0f), color, label);
        };

        if (mode == ViewMode::Mode2D) {
            drawArrow(center, ImVec2(center.x, center.y - 32.0f), IM_COL32(72, 220, 120, 255), "N");
            drawArrow(center, ImVec2(center.x + 36.0f, center.y), IM_COL32(240, 82, 82, 255), "E");
        } else {
            struct Vec3 { float x; float y; float z; };
            auto dot = [](Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
            auto cross = [](Vec3 a, Vec3 b) { return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; };
            auto normalize = [dot](Vec3 v) {
                const float len = std::sqrt(std::max(0.000001f, dot(v, v)));
                return Vec3{v.x / len, v.y / len, v.z / len};
            };

            const float yaw = status.orbitYawDegrees * 3.14159265358979323846f / 180.0f;
            const float pitch = status.orbitPitchDegrees * 3.14159265358979323846f / 180.0f;
            const Vec3 eyeDir{std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
            const Vec3 forward = normalize(Vec3{-eyeDir.x, -eyeDir.y, -eyeDir.z});
            const Vec3 right = normalize(cross(forward, Vec3{0.0f, 0.0f, 1.0f}));
            const Vec3 up = cross(right, forward);
            auto projectAxis = [dot, right, up, center](Vec3 axis, float length) {
                return ImVec2(center.x + dot(axis, right) * length, center.y - dot(axis, up) * length);
            };

            drawArrow(center, projectAxis(Vec3{1.0f, 0.0f, 0.0f}, 34.0f), IM_COL32(240, 82, 82, 255), "E");
            drawArrow(center, projectAxis(Vec3{0.0f, 1.0f, 0.0f}, 34.0f), IM_COL32(72, 220, 120, 255), "N");
            drawArrow(center, projectAxis(Vec3{0.0f, 0.0f, 1.0f}, 34.0f), IM_COL32(88, 148, 255, 255), "Z");
        }

        ImGui::End();
    }

private:
    EditorRegistry& registry_;
};
} // namespace

EditorShell::EditorShell() {
    loadLayoutSettings();
}

EditorShell::~EditorShell() {
    EditorContext context = makeContext();
    pluginManager_.unloadAll(context);
    // Important for DLL plugins: objects allocated by plugin modules must be
    // destroyed while their dynamic libraries are still loaded. PluginManager
    // keeps libraries open until its own destructor, so clearing the registry
    // here safely releases plugin-created panels/windows/layers first.
    registry_.clear();
}

void EditorShell::addTrackpadPinchDelta(float magnification) {
    pendingTrackpadPinchDelta_ += magnification;
}

void EditorShell::addTrackpadScrollDelta(float deltaY) {
    pendingTrackpadScrollDelta_ += deltaY;
}

EditorContext EditorShell::makeContext() {
    return EditorContext(registry_, layoutState_, viewportStatus_, viewportInput_, visibleRect_, viewMode_, selectionManager_, sceneRegistry_);
}

std::string EditorShell::windowTitle() const {
    if (!registry_.mainMenuPanels().empty() && activePanelIndex_ >= 0 && activePanelIndex_ < static_cast<int>(registry_.mainMenuPanels().size())) {
        return std::string("Hephaiston - ") + registry_.mainMenuPanels()[activePanelIndex_].displayName.c_str();
    }
    return "Hephaiston";
}

void EditorShell::initializeCoreRegistry() {
    registerCoreExtensions();
    loadPluginsFromKnownDirectories();
}

void EditorShell::registerCoreExtensions() {
    for (const auto& [id, title] : std::array<std::pair<const char*, const char*>, 8> {{
             {"window.project_settings", "Project Settings"},
             {"window.plugin_manager", "Plugin Manager"},
             {"window.layer_manager", "Layer Manager"},
             {"window.coordinate_inspector", "Coordinate Inspector"},
             {"window.rendering_settings", "Rendering Settings"},
             {"window.console", "Console"},
             {"window.debug_information", "Debug Information"},
             {"window.about", "About"},
         }}) {
        registry_.registerWindow(std::make_unique<GenericEditorWindow>(id, title));
    }

    registry_.registerOverlay(std::make_unique<ViewModeToggleOverlay>());
    registry_.registerOverlay(std::make_unique<ViewportStatusOverlay>());
    registry_.registerOverlay(std::make_unique<ViewGizmoOverlay>(registry_));

    registry_.registerCommand({"command.focus_view", "Focus in View", [] {}});
    registry_.registerCommand({"command.generate_volume", "Generate Volume", [] {}});

}

void EditorShell::loadPluginsFromKnownDirectories() {
    EditorContext context = makeContext();
    pluginManager_.loadAllFromDirectory(context, std::filesystem::current_path() / "plugins");
    pluginManager_.loadAllFromDirectory(context, std::filesystem::current_path() / "build" / "plugins");
}

void EditorShell::unloadAllPlugins() {
    EditorContext context = makeContext();
    pluginManager_.unloadAll(context);
    // Plugins can own every extension point in the registry. Destroy those
    // objects while their libraries are still open, then recreate Core UI.
    registry_.clear();
    pluginManager_.releaseUnloadedLibraries();
    activePanelIndex_ = 0;
    selectedHierarchyItem_.clear();
    registerCoreExtensions();
}

void EditorShell::draw(ViewportRenderer& viewportRenderer, bool& shouldClose) {
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    visibleRect_ = calculateVisibleRect(displaySize);

    const float menuHeight = ImGui::GetFrameHeight();
    const float statusHeight = layoutState_.showStatusBar ? kStatusBarHeight : 0.0f;
    updateViewportInput();

    const int fboWidth = std::max(1, static_cast<int>(std::round(displaySize.x)));
    const int fboHeight = std::max(1, static_cast<int>(std::round(displaySize.y - menuHeight - statusHeight)));
    viewportRenderer.resize(fboWidth, fboHeight);
    viewportRenderer.render(viewMode_, viewportStatus_, registry_.viewportRenderSettings(), registry_.viewportSceneLayers());

    drawMainMenu(shouldClose);
    drawViewportBackground(viewportRenderer);
    drawLeftPanel();
    drawRightPanel();
    drawOverlays();
    drawFloatingWindows();
    if (layoutState_.showStatusBar) {
        drawStatusBar();
    }
}

void EditorShell::loadLayoutSettings() {
    std::ifstream in(settingsPath());
    if (!in) {
        return;
    }
    std::string key;
    while (in >> key) {
        if (key == "leftPanelCollapsed") in >> layoutState_.leftPanelCollapsed;
        else if (key == "rightPanelCollapsed") in >> layoutState_.rightPanelCollapsed;
        else if (key == "leftPanelWidth") in >> layoutState_.leftPanelWidth;
        else if (key == "rightPanelWidth") in >> layoutState_.rightPanelWidth;
        else if (key == "showStatusBar") in >> layoutState_.showStatusBar;
        else if (key == "maxFps") in >> maxFps_;
        else if (key == "zoomSensitivity") in >> registry_.viewportNavigationSettings().zoomSensitivity;
        else if (key == "moveSensitivity") in >> registry_.viewportNavigationSettings().moveSensitivity;
        else if (key == "trackpadZoomGestureMode") { int mode = 0; in >> mode; registry_.viewportNavigationSettings().trackpadZoomGestureMode = mode == 1 ? TrackpadZoomGestureMode::Pinch : TrackpadZoomGestureMode::TwoFingerScroll; }
    }
    if (maxFps_ <= 0) {
        maxFps_ = 0;
    }
    registry_.viewportNavigationSettings().zoomSensitivity = std::clamp(registry_.viewportNavigationSettings().zoomSensitivity, 0.1f, 4.0f);
    registry_.viewportNavigationSettings().moveSensitivity = std::clamp(registry_.viewportNavigationSettings().moveSensitivity, 0.1f, 4.0f);
    layoutState_.leftPanelWidth = std::clamp(layoutState_.leftPanelWidth, kMinPanelWidth, kMaxPanelWidth);
    layoutState_.rightPanelWidth = std::clamp(layoutState_.rightPanelWidth, kMinPanelWidth, kMaxPanelWidth);
}

void EditorShell::saveLayoutSettings() const {
    std::ofstream out(settingsPath());
    if (!out) {
        return;
    }
    out << "leftPanelCollapsed " << layoutState_.leftPanelCollapsed << '\n';
    out << "rightPanelCollapsed " << layoutState_.rightPanelCollapsed << '\n';
    out << "leftPanelWidth " << layoutState_.leftPanelWidth << '\n';
    out << "rightPanelWidth " << layoutState_.rightPanelWidth << '\n';
    out << "showStatusBar " << layoutState_.showStatusBar << '\n';
    out << "maxFps " << maxFps_ << '\n';
    out << "zoomSensitivity " << registry_.viewportNavigationSettings().zoomSensitivity << '\n';
    out << "moveSensitivity " << registry_.viewportNavigationSettings().moveSensitivity << '\n';
    out << "trackpadZoomGestureMode " << (registry_.viewportNavigationSettings().trackpadZoomGestureMode == TrackpadZoomGestureMode::Pinch ? 1 : 0) << '\n';
}

void EditorShell::drawPluginMenuItems(std::string_view menuName) {
    for (auto& item : registry_.menuItems()) {
        if (item.menuName == menuName) {
            const char* shortcut = item.shortcut.empty() ? nullptr : item.shortcut.c_str();
            if (ImGui::MenuItem(item.label.c_str(), shortcut, item.selected, item.enabled)) {
                item.execute();
            }
        }
    }
}

void EditorShell::drawMainMenu(bool& shouldClose) {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    bool requestPluginRescan = false;
    bool requestPluginUnloadAll = false;
    std::string requestedPluginPath;

    const EditorMenuVisibility& menuVisibility = registry_.menuVisibility();
    if (menuVisibility.showFile && ImGui::BeginMenu("File")) {
        if (!registry_.windows().empty() && ImGui::MenuItem("Project Settings")) registry_.windows()[0]->open() = true;
        drawPluginMenuItems("File");
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) shouldClose = true;
        ImGui::EndMenu();
    }
    if (menuVisibility.showEdit && ImGui::BeginMenu("Edit")) {
        ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
        ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
        drawPluginMenuItems("Edit");
        ImGui::EndMenu();
    }
    if (menuVisibility.showView && ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("2D Orthographic", nullptr, viewMode_ == ViewMode::Mode2D)) viewMode_ = ViewMode::Mode2D;
        if (ImGui::MenuItem("3D Perspective", nullptr, viewMode_ == ViewMode::Mode3D)) viewMode_ = ViewMode::Mode3D;
        if (ImGui::MenuItem("Reset View", "R")) resetViewportCamera();
        ImGui::Separator();
        if (ImGui::MenuItem("Horizontal Grid", nullptr, registry_.viewportRenderSettings().showHorizontalGrid)) {
            registry_.viewportRenderSettings().showHorizontalGrid = !registry_.viewportRenderSettings().showHorizontalGrid;
        }
        if (ImGui::MenuItem("Origin XYZ Axes", nullptr, registry_.viewportRenderSettings().showOriginAxes)) {
            registry_.viewportRenderSettings().showOriginAxes = !registry_.viewportRenderSettings().showOriginAxes;
        }
        if (ImGui::BeginMenu("Sensitivity")) {
            auto& navigation = registry_.viewportNavigationSettings();
            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::SliderFloat("Zoom", &navigation.zoomSensitivity, 0.1f, 4.0f, "%.2fx")) saveLayoutSettings();
            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::SliderFloat("Move", &navigation.moveSensitivity, 0.1f, 4.0f, "%.2fx")) saveLayoutSettings();
            if (ImGui::MenuItem("Reset")) { navigation = {}; saveLayoutSettings(); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Trackpad Zoom")) {
            auto& navigation = registry_.viewportNavigationSettings();
            if (ImGui::MenuItem("Two-finger Vertical Scroll", nullptr, navigation.trackpadZoomGestureMode == TrackpadZoomGestureMode::TwoFingerScroll)) {
                navigation.trackpadZoomGestureMode = TrackpadZoomGestureMode::TwoFingerScroll;
                saveLayoutSettings();
            }
            if (ImGui::MenuItem("Pinch In / Out", nullptr, navigation.trackpadZoomGestureMode == TrackpadZoomGestureMode::Pinch)) {
                navigation.trackpadZoomGestureMode = TrackpadZoomGestureMode::Pinch;
                saveLayoutSettings();
            }
            ImGui::Separator();
            ImGui::TextDisabled("Pinch requires native trackpad support.");
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Status Bar", nullptr, layoutState_.showStatusBar)) {
            layoutState_.showStatusBar = !layoutState_.showStatusBar;
            saveLayoutSettings();
        }
        drawPluginMenuItems("View");
        ImGui::EndMenu();
    }
    if (menuVisibility.showPlugins && ImGui::BeginMenu("Plugins")) {
        if (ImGui::MenuItem("Load Plugin DLL...")) {
            ImGui::OpenPopup("##LoadPluginDllPopup");
        }
        if (ImGui::MenuItem("Rescan Plugin Directories")) {
            requestPluginRescan = true;
        }
        if (ImGui::MenuItem("Unload All Loaded Plugins", nullptr, false, !pluginManager_.loadedPlugins().empty())) {
            requestPluginUnloadAll = true;
        }
        if (!pluginManager_.loadedPlugins().empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("Loaded DLL plugins");
            for (const auto& plugin : pluginManager_.loadedPlugins()) {
                ImGui::BulletText("%s %s", plugin.displayName, plugin.version);
            }
            ImGui::Separator();
        }
        if (!pluginManager_.loadErrors().empty()) {
            ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "Last DLL load error:");
            ImGui::TextWrapped("%s", pluginManager_.loadErrors().back().c_str());
            ImGui::Separator();
        }
        if (!registry_.mainMenuPanels().empty()) {
            for (int i = 0; i < static_cast<int>(registry_.mainMenuPanels().size()); ++i) {
                if (ImGui::MenuItem(registry_.mainMenuPanels()[i].displayName.c_str(), nullptr, i == activePanelIndex_)) {
                    activePanelIndex_ = i;
                }
            }
            ImGui::Separator();
        }
        drawPluginMenuItems("Plugins");
        ImGui::EndMenu();
    }
    if (menuVisibility.showWindow && ImGui::BeginMenu("Window")) {
        for (auto& window : registry_.windows()) {
            if (ImGui::MenuItem(window->displayName(), nullptr, window->open())) {
                window->open() = true;
            }
        }
        for (auto& window : registry_.floatingWindows()) {
            if (ImGui::MenuItem(window.displayName.c_str(), nullptr, window.open)) {
                window.open = true;
            }
        }
        drawPluginMenuItems("Window");
        ImGui::EndMenu();
    }
    if (menuVisibility.showHelp && ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About")) {
            for (auto& window : registry_.windows()) {
                if (std::string_view(window->id()) == "window.about") window->open() = true;
            }
        }
        drawPluginMenuItems("Help");
        ImGui::EndMenu();
    }

    std::set<std::string> customMenus;
    for (const auto& item : registry_.menuItems()) {
        if (item.menuName != "File" && item.menuName != "Edit" && item.menuName != "View" &&
            item.menuName != "Plugins" && item.menuName != "Window" && item.menuName != "Help") {
            customMenus.insert(item.menuName);
        }
    }
    for (const auto& menuName : customMenus) {
        if (ImGui::BeginMenu(menuName.c_str())) {
            drawPluginMenuItems(menuName);
            ImGui::EndMenu();
        }
    }

    EditorContext context = makeContext();
    if (!registry_.viewportTools().empty() && ImGui::BeginMenu("Tools")) {
        const IViewportTool* activeTool = registry_.activeViewportTool();
        for (const auto& tool : registry_.viewportTools()) {
            if (!tool) {
                continue;
            }
            const bool selected = activeTool && std::string_view(activeTool->id()) == tool->id();
            if (ImGui::MenuItem(tool->displayName(), nullptr, selected)) {
                registry_.setActiveViewportTool(tool->id(), context);
            }
        }
        ImGui::EndMenu();
    }

    for (auto& contributor : registry_.menuBarContributors()) {
        if (contributor.contributor) {
            contributor.contributor->draw(context);
        }
    }

    if (menuVisibility.showFpsControl) {
        ImGuiIO& io = ImGui::GetIO();
        constexpr float fpsButtonWidth = 122.0f;
        const float rightX = std::max(ImGui::GetCursorPosX() + 8.0f, ImGui::GetWindowWidth() - fpsButtonWidth - 8.0f);
        ImGui::SetCursorPosX(rightX);

        const ImVec2 fpsButtonSize(fpsButtonWidth, ImGui::GetFrameHeight());
        const bool fpsPressed = ImGui::Button("##MenuBarFpsButtonStable", fpsButtonSize);
        const ImVec2 buttonMin = ImGui::GetItemRectMin();
        const ImVec2 buttonMax = ImGui::GetItemRectMax();
        char fpsLabel[32] = {};
        std::snprintf(fpsLabel, sizeof(fpsLabel), "FPS: %.1f", io.Framerate);
        const ImVec2 textSize = ImGui::CalcTextSize(fpsLabel);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(buttonMin.x + (buttonMax.x - buttonMin.x - textSize.x) * 0.5f,
                   buttonMin.y + (buttonMax.y - buttonMin.y - textSize.y) * 0.5f),
            ImGui::GetColorU32(ImGuiCol_Text),
            fpsLabel);
        if (fpsPressed) {
            ImGui::OpenPopup("##MaxFpsPopup");
        }
        if (ImGui::BeginPopup("##MaxFpsPopup")) {
            ImGui::TextUnformatted("Max FPS");
            ImGui::Separator();
            ImGui::SetNextItemWidth(140.0f);
            if (ImGui::InputInt("##MaxFpsInput", &maxFps_, 0, 0)) {
                saveLayoutSettings();
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                saveLayoutSettings();
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(maxFps_ <= 0 ? "Unlimited" : "fps");
            ImGui::TextDisabled("0 or less = Unlimited");
            ImGui::EndPopup();
        }
    }

    ImGui::EndMainMenuBar();

    if (ImGui::BeginPopup("##LoadPluginDllPopup")) {
        static char pluginPath[1024] = {};
        ImGui::TextUnformatted("Load a DLL/dylib/shared library");
        ImGui::SetNextItemWidth(440.0f);
        ImGui::InputText("Path", pluginPath, sizeof(pluginPath));
        if (ImGui::Button("Load") && pluginPath[0] != '\0') {
            requestedPluginPath = pluginPath;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (requestPluginUnloadAll) {
        unloadAllPlugins();
    } else if (!requestedPluginPath.empty()) {
        EditorContext context = makeContext();
        pluginManager_.loadPlugin(context, requestedPluginPath);
    } else if (requestPluginRescan) {
        loadPluginsFromKnownDirectories();
    }
}


void EditorShell::drawViewportBackground(ViewportRenderer& viewportRenderer) {
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const float menuHeight = ImGui::GetFrameHeight();
    const float statusHeight = layoutState_.showStatusBar ? kStatusBarHeight : 0.0f;
    ImGui::SetNextWindowPos(ImVec2(0.0f, menuHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(displaySize.x, std::max(1.0f, displaySize.y - menuHeight - statusHeight)), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
    ImGui::Begin("##MainFBOBackground", nullptr, flags);
    ImGui::Image(static_cast<ImTextureID>(static_cast<std::uintptr_t>(viewportRenderer.texture())), ImGui::GetContentRegionAvail(), ImVec2(0, 1), ImVec2(1, 0));
    ImGui::End();
}

void EditorShell::drawLeftPanel() {
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const float menuHeight = ImGui::GetFrameHeight();
    const float statusHeight = layoutState_.showStatusBar ? kStatusBarHeight : 0.0f;
    const float panelWidth = layoutState_.leftPanelCollapsed ? kCollapsedPanelWidth : layoutState_.leftPanelWidth;
    const float height = std::max(1.0f, displaySize.y - menuHeight - statusHeight);

    ImGui::SetNextWindowPos(ImVec2(0.0f, menuHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, height), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("##LeftSidePanel", nullptr, flags);
    if (layoutState_.leftPanelCollapsed) {
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetContentRegionAvail().x - kPanelToggleButtonSize.x) * 0.5f));
        if (drawPanelToggleButton("##LeftPanelExpandButton", true)) {
            layoutState_.leftPanelCollapsed = false;
            saveLayoutSettings();
        }
    } else {
        ImGui::SetCursorPosX(std::max(0.0f, ImGui::GetContentRegionAvail().x - kPanelToggleButtonSize.x - kPanelToggleEdgePadding));
        if (drawPanelToggleButton("##LeftPanelCollapseButton", false)) {
            layoutState_.leftPanelCollapsed = true;
            saveLayoutSettings();
        }
        if (!registry_.mainMenuPanels().empty()) {
            ImGui::SeparatorText("Plugins");
            activePanelIndex_ = std::clamp(activePanelIndex_, 0, static_cast<int>(registry_.mainMenuPanels().size()) - 1);
            const char* current = registry_.mainMenuPanels()[activePanelIndex_].displayName.c_str();
            if (ImGui::BeginCombo("Active", current)) {
                for (int i = 0; i < static_cast<int>(registry_.mainMenuPanels().size()); ++i) {
                    const bool selected = i == activePanelIndex_;
                    if (ImGui::Selectable(registry_.mainMenuPanels()[i].displayName.c_str(), selected)) {
                        activePanelIndex_ = i;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::Separator();
            registry_.mainMenuPanels()[activePanelIndex_].panel->draw();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();

    if (!layoutState_.leftPanelCollapsed) {
        handleSplitter(layoutState_.leftPanelWidth, menuHeight, height, true);
    }
}

void EditorShell::drawRightPanel() {
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const float menuHeight = ImGui::GetFrameHeight();
    const float statusHeight = layoutState_.showStatusBar ? kStatusBarHeight : 0.0f;
    const float panelWidth = layoutState_.rightPanelCollapsed ? kCollapsedPanelWidth : layoutState_.rightPanelWidth;
    const float height = std::max(1.0f, displaySize.y - menuHeight - statusHeight);
    const float x = displaySize.x - panelWidth;

    ImGui::SetNextWindowPos(ImVec2(x, menuHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, height), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("##RightSidePanel", nullptr, flags);
    if (!layoutState_.rightPanelCollapsed) {
        std::vector<EditorHierarchyItem> hierarchyItems;
        for (auto& provider : registry_.hierarchyProviders()) {
            if (provider) {
                provider->collectHierarchy(hierarchyItems);
            }
        }
        std::vector<SceneObject> sceneProviderObjects;
        for (auto& provider : registry_.sceneProviders()) {
            if (provider) {
                provider->collectSceneObjects(sceneProviderObjects);
            }
        }
        if (!hierarchyRoot_.name.empty() || !hierarchyItems.empty() || !sceneRegistry_.rootObjects().empty() || !sceneProviderObjects.empty() || !registry_.hierarchyPanels().empty()) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + kPanelToggleButtonSize.y + 6.0f);
            ImGui::SeparatorText("Hierarchy");
            if (!hierarchyRoot_.name.empty()) {
                drawHierarchyNode(hierarchyRoot_);
            }
            for (const auto& object : sceneRegistry_.rootObjects()) {
                drawSceneObject(object);
            }
            for (const auto& object : sceneProviderObjects) {
                drawSceneObject(object);
            }
            for (const auto& item : hierarchyItems) {
                drawHierarchyItem(item);
            }
            for (auto& panel : registry_.hierarchyPanels()) {
                if (panel.panel) {
                    if (!panel.displayName.empty()) {
                        ImGui::SeparatorText(panel.displayName.c_str());
                    }
                    panel.panel->draw();
                }
            }
            if (const SelectionItem* selected = selectionManager_.primary()) {
                EditorContext context = makeContext();
                for (auto& panel : registry_.propertiesPanels()) {
                    if (panel && panel->canInspect(*selected)) {
                        ImGui::SeparatorText("Properties");
                        panel->draw(context, *selected);
                    }
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();

    if (!layoutState_.rightPanelCollapsed) {
        handleSplitter(displaySize.x - layoutState_.rightPanelWidth - kSplitterWidth, menuHeight, height, false);
    }

    // Draw the right panel toggle on the foreground with manual hit-testing so it is never
    // clipped or occluded by the panel body, splitter, or viewport overlays.
    const ImVec2 togglePos(
        layoutState_.rightPanelCollapsed ? x + (panelWidth - kPanelToggleButtonSize.x) * 0.5f : x + kPanelToggleEdgePadding,
        menuHeight + 8.0f);
    if (layoutState_.rightPanelCollapsed) {
        if (drawPanelToggleButtonAt("##RightPanelExpandButton", togglePos, false)) {
            layoutState_.rightPanelCollapsed = false;
            saveLayoutSettings();
        }
    } else {
        if (drawPanelToggleButtonAt("##RightPanelCollapseButton", togglePos, true)) {
            layoutState_.rightPanelCollapsed = true;
            saveLayoutSettings();
        }
    }

}

void EditorShell::drawStatusBar() {
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, displaySize.y - kStatusBarHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(displaySize.x, kStatusBarHeight), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("##StatusBar", nullptr, flags);
    ImGui::Text("Hephaiston Core | FBO %s | Hover:%s Orbit:%s Pan:%s Wheel:%.1f | Selected: %s",
                viewMode_ == ViewMode::Mode3D ? "3D" : "2D",
                viewportInput_.hovered ? "yes" : "no",
                viewportInput_.rotating ? "yes" : "no",
                viewportInput_.panning ? "yes" : "no",
                viewportInput_.wheel,
                selectedHierarchyItem_.c_str());
    for (const auto& item : registry_.statusBarItems()) {
        if (item.visible) {
            ImGui::SameLine();
            ImGui::TextUnformatted("|");
            ImGui::SameLine();
            ImGui::TextUnformatted(item.text.c_str());
        }
    }
    for (auto& widget : registry_.statusBarWidgets()) {
        if (widget.visible && widget.widget) {
            ImGui::SameLine();
            ImGui::TextUnformatted("|");
            ImGui::SameLine();
            widget.widget->draw();
        }
    }
    ImGui::End();
}

void EditorShell::drawFloatingWindows() {
    for (auto& window : registry_.windows()) {
        if (!window->open()) {
            continue;
        }
        ImGui::SetNextWindowSize(ImVec2(360.0f, 240.0f), ImGuiCond_FirstUseEver);
        bool open = window->open();
        if (ImGui::Begin(window->displayName(), &open)) {
            window->draw();
        }
        ImGui::End();
        window->open() = open;
    }
    for (auto& window : registry_.floatingWindows()) {
        if (!window.open || !window.window) {
            continue;
        }
        ImGui::SetNextWindowSize(ImVec2(360.0f, 240.0f), ImGuiCond_FirstUseEver);
        bool open = window.open;
        if (ImGui::Begin(window.displayName.c_str(), &open)) {
            window.window->draw();
        }
        ImGui::End();
        window.open = open;
    }
}

void EditorShell::drawOverlays() {
    for (auto& overlay : registry_.overlays()) {
        EditorContext context = makeContext();
        if (std::string_view(overlay->id()) == "overlay.viewport_status" && !registry_.viewportRenderSettings().showScaleBar) {
            continue;
        }
        if (std::string_view(overlay->id()) == "overlay.view_mode_toggle" && !registry_.viewportRenderSettings().showViewModeToggle) {
            continue;
        }
        if (IViewportTool* activeTool = registry_.activeViewportTool();
            activeTool && activeTool->blocksDefaultViewportNavigation(context) &&
            (std::string_view(overlay->id()) == "overlay.view_gizmo" || std::string_view(overlay->id()) == "overlay.view_mode_toggle")) {
            continue;
        }
        overlay->draw(visibleRect_, viewportStatus_, viewMode_);
    }
    if (IViewportTool* activeTool = registry_.activeViewportTool()) {
        EditorContext context = makeContext();
        activeTool->drawToolbar(context);
    }
}

void EditorShell::drawHierarchyNode(const HierarchyNode& node) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (selectedHierarchyItem_ == node.name) flags |= ImGuiTreeNodeFlags_Selected;

    const bool open = ImGui::TreeNodeEx(node.name.c_str(), flags);
    if (ImGui::IsItemClicked()) {
        selectedHierarchyItem_ = node.name;
    }
    if (ImGui::BeginPopupContextItem()) {
        ImGui::TextDisabled("%s", node.name.c_str());
        ImGui::Separator();
        if (ImGui::MenuItem("Rename")) {}
        if (ImGui::MenuItem("Duplicate")) {}
        if (ImGui::MenuItem("Delete")) {}
        if (ImGui::MenuItem("Hide / Show")) {}
        if (ImGui::MenuItem("Focus in View")) {}
        ImGui::EndPopup();
    }

    if (!node.children.empty() && open) {
        for (const auto& child : node.children) {
            drawHierarchyNode(child);
        }
        ImGui::TreePop();
    }
}

void EditorShell::drawHierarchyItem(const EditorHierarchyItem& item) {
    const std::string label = item.displayName.empty() ? item.id : item.displayName;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (item.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (selectedHierarchyItem_ == item.id) flags |= ImGuiTreeNodeFlags_Selected;

    const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
    if (ImGui::IsItemClicked()) {
        selectedHierarchyItem_ = item.id;
        selectionManager_.select({item.id, label, "HierarchyItem"});
    }
    if (ImGui::BeginPopupContextItem()) {
        ImGui::TextDisabled("%s", label.c_str());
        ImGui::Separator();
        if (ImGui::MenuItem("Rename")) {}
        if (ImGui::MenuItem("Duplicate")) {}
        if (ImGui::MenuItem("Delete")) {}
        if (ImGui::MenuItem("Hide / Show")) {}
        if (ImGui::MenuItem("Focus in View")) {}
        EditorContext context = makeContext();
        for (auto& provider : registry_.contextMenuProviders()) {
            if (provider) {
                provider->drawHierarchyContextMenu(context, item.id);
            }
        }
        ImGui::EndPopup();
    }

    if (!item.children.empty() && open) {
        for (const auto& child : item.children) {
            drawHierarchyItem(child);
        }
        ImGui::TreePop();
    }
}

void EditorShell::drawSceneObject(const SceneObject& object) {
    const std::string label = object.displayName.empty() ? object.id : object.displayName;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (object.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (selectedHierarchyItem_ == object.id) flags |= ImGuiTreeNodeFlags_Selected;
    if (!object.visible) flags |= ImGuiTreeNodeFlags_Bullet;

    const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
    if (ImGui::IsItemClicked()) {
        selectedHierarchyItem_ = object.id;
        selectionManager_.select({object.id, label, "SceneObject"});
    }
    if (ImGui::BeginPopupContextItem()) {
        ImGui::TextDisabled("%s", label.c_str());
        ImGui::Separator();
        if (ImGui::MenuItem("Rename")) {}
        if (ImGui::MenuItem("Duplicate")) {}
        if (ImGui::MenuItem("Delete")) {}
        if (ImGui::MenuItem(object.visible ? "Hide" : "Show")) {}
        if (ImGui::MenuItem("Focus in View")) {}
        EditorContext context = makeContext();
        for (auto& provider : registry_.contextMenuProviders()) {
            if (provider) {
                provider->drawHierarchyContextMenu(context, object.id);
            }
        }
        ImGui::EndPopup();
    }

    if (!object.children.empty() && open) {
        for (const auto& child : object.children) {
            drawSceneObject(child);
        }
        ImGui::TreePop();
    }
}

void EditorShell::updateViewportInput() {
    ImGuiIO& io = ImGui::GetIO();
    viewportInput_.mousePos = io.MousePos;
    const ImVec2 gizmoSize = kViewGizmoOverlaySize;
    const ImVec2 gizmoMin(visibleRect_.max.x - gizmoSize.x - 6.0f, visibleRect_.max.y - gizmoSize.y - 6.0f);
    const ImVec2 gizmoMax(visibleRect_.max.x - 6.0f, visibleRect_.max.y - 6.0f);
    const ImVec2 scaleMin(visibleRect_.min.x + 2.0f, visibleRect_.max.y - kScaleOverlaySize.y - 2.0f);
    const ImVec2 scaleMax(scaleMin.x + kScaleOverlaySize.x, scaleMin.y + kScaleOverlaySize.y);
    const ImVec2 viewModeMin(visibleRect_.min.x + 8.0f, visibleRect_.min.y + 8.0f);
    const ImVec2 viewModeMax(viewModeMin.x + kViewModeToggleOverlaySize.x, viewModeMin.y + kViewModeToggleOverlaySize.y);
    const bool mouseOverGizmo = ImGui::IsMouseHoveringRect(gizmoMin, gizmoMax, false);
    const bool mouseOverScale = ImGui::IsMouseHoveringRect(scaleMin, scaleMax, false);
    const bool mouseOverViewMode = registry_.viewportRenderSettings().showViewModeToggle &&
        ImGui::IsMouseHoveringRect(viewModeMin, viewModeMax, false);
    viewportInput_.hovered = ImGui::IsMouseHoveringRect(visibleRect_.min, visibleRect_.max, false) && !mouseOverGizmo && !mouseOverScale && !mouseOverViewMode && (!io.WantCaptureMouse || viewportDragActive_);
    viewportInput_.clicked = viewportInput_.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    constexpr float kPinchMagnificationToZoomDelta = 6.0f;
    const bool usePinch = registry_.viewportNavigationSettings().trackpadZoomGestureMode == TrackpadZoomGestureMode::Pinch;
    const bool hasTrackpadScroll = std::abs(pendingTrackpadScrollDelta_) > 0.0001f;
    viewportInput_.wheel = viewportInput_.hovered
        // Pinch mode suppresses only macOS's precise trackpad scroll events;
        // a conventional mouse wheel remains available in both modes.
        ? (usePinch && hasTrackpadScroll ? pendingTrackpadPinchDelta_ * kPinchMagnificationToZoomDelta
                                         : (usePinch && pendingTrackpadPinchDelta_ != 0.0f ? pendingTrackpadPinchDelta_ * kPinchMagnificationToZoomDelta : io.MouseWheel))
        : 0.0f;
    pendingTrackpadPinchDelta_ = 0.0f;
    pendingTrackpadScrollDelta_ = 0.0f;
    viewportInput_.dragging = false;
    viewportInput_.panning = false;
    viewportInput_.rotating = false;

    EditorContext inputContext = makeContext();
    const bool navigationBlocked = registry_.activeViewportTool() && registry_.activeViewportTool()->blocksDefaultViewportNavigation(inputContext);
    const bool navigationHandledByTool = registry_.activeViewportTool() && registry_.activeViewportTool()->handlesViewportNavigation(inputContext);

    if (!navigationBlocked && !navigationHandledByTool && !viewportDragActive_ && viewportInput_.hovered) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            viewportDragActive_ = true;
            viewportDragButton_ = ImGuiMouseButton_Right;
            previousViewportMousePos_ = io.MousePos;
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
            viewportDragActive_ = true;
            viewportDragButton_ = ImGuiMouseButton_Middle;
            previousViewportMousePos_ = io.MousePos;
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            viewportDragActive_ = true;
            viewportDragButton_ = ImGuiMouseButton_Left;
            previousViewportMousePos_ = io.MousePos;
        }
    }

    const bool activeButtonDown = !navigationBlocked && !navigationHandledByTool && viewportDragActive_ && ImGui::IsMouseDown(viewportDragButton_);
    if (viewportDragActive_ && !activeButtonDown) {
        viewportDragActive_ = false;
        viewportInput_.dragDelta = ImVec2(0.0f, 0.0f);
    }

    if (activeButtonDown) {
        const auto& navigation = registry_.viewportNavigationSettings();
        const float moveSensitivity = viewMode_ == ViewMode::Mode3D
            ? navigation.effectiveOrbitMoveSensitivity(viewportStatus_.orbitDistanceMeters)
            : navigation.effectiveMoveSensitivity();
        const ImVec2 delta(io.MousePos.x - previousViewportMousePos_.x, io.MousePos.y - previousViewportMousePos_.y);
        previousViewportMousePos_ = io.MousePos;
        viewportInput_.dragging = std::abs(delta.x) > 0.0f || std::abs(delta.y) > 0.0f;
        viewportInput_.dragDelta = delta;

        if (viewMode_ == ViewMode::Mode3D && viewportDragButton_ == ImGuiMouseButton_Left && !io.KeyShift) {
            viewportInput_.rotating = true;
            viewportStatus_.orbitYawDegrees -= delta.x * 0.28f * moveSensitivity;
            viewportStatus_.orbitPitchDegrees = std::clamp(viewportStatus_.orbitPitchDegrees + delta.y * 0.22f * moveSensitivity, -82.0f, 82.0f);
        } else {
            viewportInput_.panning = true;
            if (viewMode_ == ViewMode::Mode2D) {
                const float mpp = static_cast<float>(viewportStatus_.metersPerPixel);
                viewportStatus_.panMeters.x -= delta.x * mpp * moveSensitivity;
                viewportStatus_.panMeters.y += delta.y * mpp * moveSensitivity;
            } else {
                const float yaw = viewportStatus_.orbitYawDegrees * 3.14159265358979323846f / 180.0f;
                const float pitch = viewportStatus_.orbitPitchDegrees * 3.14159265358979323846f / 180.0f;
                const float scale = std::max(0.02f, viewportStatus_.orbitDistanceMeters * 0.0018f) * moveSensitivity;
                const float rightX = -std::sin(yaw);
                const float rightY = std::cos(yaw);
                const float upX = -std::cos(yaw) * std::sin(pitch);
                const float upY = -std::sin(yaw) * std::sin(pitch);
                const float upZ = std::cos(pitch);
                viewportStatus_.targetX -= rightX * delta.x * scale;
                viewportStatus_.targetY -= rightY * delta.x * scale;
                viewportStatus_.targetX += upX * delta.y * scale;
                viewportStatus_.targetY += upY * delta.y * scale;
                viewportStatus_.targetZ += upZ * delta.y * scale;
            }
        }
    }

    const float width = std::max(1.0f, visibleRect_.max.x - visibleRect_.min.x);
    const float height = std::max(1.0f, visibleRect_.max.y - visibleRect_.min.y);
    const ImVec2 cursorOffset(io.MousePos.x - visibleRect_.min.x - width * 0.5f,
                              height * 0.5f - (io.MousePos.y - visibleRect_.min.y));

    if (!navigationBlocked && !navigationHandledByTool && viewportInput_.hovered && std::abs(viewportInput_.wheel) > 0.0f) {
        if (viewMode_ == ViewMode::Mode2D) {
            const float beforeMpp = static_cast<float>(viewportStatus_.metersPerPixel);
            const ImVec2 worldUnderCursor(viewportStatus_.panMeters.x + cursorOffset.x * beforeMpp,
                                          viewportStatus_.panMeters.y + cursorOffset.y * beforeMpp);
            const double factor = std::pow(1.12, static_cast<double>(viewportInput_.wheel) * registry_.viewportNavigationSettings().effectiveZoomSensitivity());
            viewportStatus_.zoom = std::clamp(viewportStatus_.zoom * factor, 0.000001, 4096.0);
            viewportStatus_.metersPerPixel = 1.0 / viewportStatus_.zoom;
            const float afterMpp = static_cast<float>(viewportStatus_.metersPerPixel);
            viewportStatus_.panMeters.x = worldUnderCursor.x - cursorOffset.x * afterMpp;
            viewportStatus_.panMeters.y = worldUnderCursor.y - cursorOffset.y * afterMpp;
        } else {
            // Match the 2D zoom response: 3D distance is the reciprocal of
            // 2D magnification, so both use the same sensitivity curve.
            const double factor = std::pow(1.0 / 1.12, static_cast<double>(viewportInput_.wheel) *
                registry_.viewportNavigationSettings().effectiveOrbitZoomSensitivity(viewportStatus_.orbitDistanceMeters));
            viewportStatus_.orbitDistanceMeters = std::clamp(static_cast<float>(viewportStatus_.orbitDistanceMeters * factor), 0.2f, 10000000.0f);
            viewportStatus_.zoom = 100.0 / static_cast<double>(viewportStatus_.orbitDistanceMeters);
            viewportStatus_.metersPerPixel = viewportStatus_.orbitDistanceMeters / 900.0;
        }
    }

    if (!navigationBlocked && !navigationHandledByTool && viewportInput_.hovered && ImGui::IsKeyPressed(ImGuiKey_R) && !io.WantCaptureKeyboard) {
        resetViewportCamera();
    }

    viewportStatus_.cursorMeters = ImVec2(viewportStatus_.panMeters.x + cursorOffset.x * static_cast<float>(viewportStatus_.metersPerPixel),
                                         viewportStatus_.panMeters.y + cursorOffset.y * static_cast<float>(viewportStatus_.metersPerPixel));

    const double desiredGridPixels = 72.0;
    const double rawGrid = std::max(0.01, desiredGridPixels * viewportStatus_.metersPerPixel);
    const double power = std::pow(10.0, std::floor(std::log10(rawGrid)));
    const double normalized = rawGrid / power;
    double step = 1.0;
    if (normalized > 5.0) step = 10.0;
    else if (normalized > 2.0) step = 5.0;
    else if (normalized > 1.0) step = 2.0;
    viewportStatus_.gridSizeMeters = step * power;
    const double denominator = std::round(8192.0 / std::max(0.000001, viewportStatus_.zoom));
    viewportStatus_.scaleDenominator = static_cast<int>(std::clamp(denominator, 10.0, 2147483647.0));

    if (IViewportTool* activeTool = registry_.activeViewportTool()) {
        EditorContext context = makeContext();
        activeTool->onViewportInput(context);
    }
}

void EditorShell::resetViewportCamera() {
    viewportStatus_.zoom = 16.4;
    viewportStatus_.metersPerPixel = 1.0 / viewportStatus_.zoom;
    viewportStatus_.gridSizeMeters = 1.0;
    viewportStatus_.scaleDenominator = 500;
    viewportStatus_.panMeters = ImVec2(0.0f, 0.0f);
    viewportStatus_.orbitYawDegrees = 45.0f;
    viewportStatus_.orbitPitchDegrees = 35.0f;
    viewportStatus_.orbitDistanceMeters = 42.0f;
    viewportStatus_.targetX = 0.0f;
    viewportStatus_.targetY = 0.0f;
    viewportStatus_.targetZ = 2.0f;
    viewportDragActive_ = false;
}

void EditorShell::handleSplitter(float x, float y, float height, bool leftSide) {
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kSplitterWidth, height), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav;
    ImGui::Begin(leftSide ? "##LeftSplitter" : "##RightSplitter", nullptr, flags);
    ImGui::InvisibleButton(leftSide ? "##LeftSplitterButton" : "##RightSplitterButton", ImVec2(kSplitterWidth, height));
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (ImGui::IsItemActive()) {
        const float delta = ImGui::GetIO().MouseDelta.x;
        if (leftSide) {
            layoutState_.leftPanelWidth = std::clamp(layoutState_.leftPanelWidth + delta, kMinPanelWidth, kMaxPanelWidth);
        } else {
            layoutState_.rightPanelWidth = std::clamp(layoutState_.rightPanelWidth - delta, kMinPanelWidth, kMaxPanelWidth);
        }
        saveLayoutSettings();
    }
    ImGui::End();
}

ViewportVisibleRect EditorShell::calculateVisibleRect(ImVec2 displaySize) const {
    const float menuHeight = ImGui::GetFrameHeight();
    const float statusHeight = layoutState_.showStatusBar ? kStatusBarHeight : 0.0f;
    const float leftOccupied = layoutState_.leftPanelCollapsed ? kCollapsedPanelWidth : layoutState_.leftPanelWidth + kSplitterWidth;
    const float rightOccupied = layoutState_.rightPanelCollapsed ? kCollapsedPanelWidth : layoutState_.rightPanelWidth + kSplitterWidth;
    return {
        ImVec2(leftOccupied, menuHeight),
        ImVec2(std::max(leftOccupied + 1.0f, displaySize.x - rightOccupied), std::max(menuHeight + 1.0f, displaySize.y - statusHeight)),
    };
}

} // namespace hephaiston
