#pragma once

#include "hephaiston/EditorRegistry.h"
#include "hephaiston/EditorTypes.h"
#include "hephaiston/PluginManager.h"
#include "hephaiston/SceneRegistry.h"
#include "hephaiston/SelectionManager.h"
#include "hephaiston/ViewportRenderer.h"

#include <string>
#include <vector>

namespace hephaiston {

class EditorContext;

class EditorShell {
public:
    EditorShell();
    ~EditorShell();

    EditorShell(const EditorShell&) = delete;
    EditorShell& operator=(const EditorShell&) = delete;

    void initializeCoreRegistry();
    void draw(ViewportRenderer& viewportRenderer, bool& shouldClose);
    // Native platform gesture bridges submit magnification deltas here. They
    // are consumed once by the next viewport input update.
    void addTrackpadPinchDelta(float magnification);
    void addTrackpadScrollDelta(float deltaY);

    [[nodiscard]] EditorRegistry& registry() { return registry_; }
    [[nodiscard]] const EditorLayoutState& layoutState() const { return layoutState_; }
    [[nodiscard]] int targetMaxFps() const { return maxFps_; }
    [[nodiscard]] std::string windowTitle() const;

private:
    struct HierarchyNode {
        std::string name;
        std::vector<HierarchyNode> children;
    };

    [[nodiscard]] EditorContext makeContext();
    void loadLayoutSettings();
    void saveLayoutSettings() const;
    void drawMainMenu(bool& shouldClose);
    void drawViewportBackground(ViewportRenderer& viewportRenderer);
    void drawLeftPanel();
    void drawRightPanel();
    void drawStatusBar();
    void drawFloatingWindows();
    void drawOverlays();
    void drawHierarchyNode(const HierarchyNode& node);
    void drawHierarchyItem(const EditorHierarchyItem& item);
    void drawSceneObject(const SceneObject& object);
    void drawPluginMenuItems(std::string_view menuName);
    void registerCoreExtensions();
    void loadPluginsFromKnownDirectories();
    void unloadAllPlugins();
    void updateViewportInput();
    void resetViewportCamera();
    void handleSplitter(float x, float y, float height, bool leftSide);
    [[nodiscard]] ViewportVisibleRect calculateVisibleRect(ImVec2 displaySize) const;

    PluginManager pluginManager_;
    EditorRegistry registry_;
    SelectionManager selectionManager_;
    SceneRegistry sceneRegistry_;
    EditorLayoutState layoutState_;
    ViewportStatus viewportStatus_;
    ViewportInputState viewportInput_;
    float pendingTrackpadPinchDelta_ = 0.0f;
    float pendingTrackpadScrollDelta_ = 0.0f;
    ViewportVisibleRect visibleRect_;
    ViewMode viewMode_ = ViewMode::Mode3D;
    bool viewportDragActive_ = false;
    ImGuiMouseButton viewportDragButton_ = ImGuiMouseButton_Left;
    ImVec2 previousViewportMousePos_ {0.0f, 0.0f};
    int activePanelIndex_ = 0;
    int maxFps_ = 60;
    std::string selectedHierarchyItem_;
    HierarchyNode hierarchyRoot_;
};

} // namespace hephaiston
