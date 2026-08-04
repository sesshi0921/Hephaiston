#pragma once

#include "hephaiston/EditorRegistry.h"
#include "hephaiston/EditorTypes.h"
#include "hephaiston/ViewportRenderer.h"

#include <string>
#include <vector>

namespace hephaiston {

class EditorShell {
public:
    EditorShell();
    ~EditorShell() = default;

    EditorShell(const EditorShell&) = delete;
    EditorShell& operator=(const EditorShell&) = delete;

    void initializeCoreRegistry();
    void draw(ViewportRenderer& viewportRenderer, bool& shouldClose);

    [[nodiscard]] EditorRegistry& registry() { return registry_; }
    [[nodiscard]] const EditorLayoutState& layoutState() const { return layoutState_; }
    [[nodiscard]] int targetMaxFps() const { return maxFps_; }
    [[nodiscard]] std::string windowTitle() const;

private:
    struct HierarchyNode {
        std::string name;
        std::vector<HierarchyNode> children;
    };

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
    void updateViewportInput();
    void resetViewportCamera();
    void handleSplitter(float x, float y, float height, bool leftSide);
    [[nodiscard]] ViewportVisibleRect calculateVisibleRect(ImVec2 displaySize) const;

    EditorRegistry registry_;
    EditorLayoutState layoutState_;
    ViewportStatus viewportStatus_;
    ViewportInputState viewportInput_;
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
