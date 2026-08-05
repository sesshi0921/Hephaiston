#pragma once

#include "hephaiston/EditorTypes.h"

#include <memory>
#include <string>
#include <vector>

namespace hephaiston {

class EditorContext;

class EditorRegistry {
public:
    void registerMainMenuPanel(std::string id, std::string displayName, std::unique_ptr<IMainMenuPanel> panel);
    void registerHierarchyPanel(std::string id, std::string displayName, std::unique_ptr<IHierarchyPanel> panel);
    void registerFloatingWindow(std::string id, std::string displayName, std::unique_ptr<IFloatingWindow> window, bool open = false);
    void registerStatusBarWidget(std::string id, std::string displayName, std::unique_ptr<IStatusBarWidget> widget, bool visible = true);
    void registerMenuBarContributor(std::string id, std::unique_ptr<IMenuBarContributor> contributor);

    // Compatibility wrappers for older add-ons.
    void registerPanel(std::unique_ptr<IEditorPanel> panel);
    void registerWindow(std::unique_ptr<IEditorWindow> window);
    void registerOverlay(std::unique_ptr<IViewportOverlay> overlay);
    void registerCommand(EditorCommand command);
    void registerCommand(std::unique_ptr<IEditorCommand> command);
    void registerMenuItem(EditorMenuItem item);
    void registerStatusBarItem(StatusBarItem item);
    void registerHierarchyProvider(std::unique_ptr<IHierarchyProvider> provider);
    void registerSceneProvider(std::unique_ptr<ISceneProvider> provider);
    void registerViewportSceneLayer(std::unique_ptr<IViewportSceneLayer> layer);
    void registerViewportTool(std::unique_ptr<IViewportTool> tool);
    void registerPropertiesPanel(std::unique_ptr<IPropertiesPanel> panel);
    void registerContextMenuProvider(std::unique_ptr<IContextMenuProvider> provider);

    bool executeCommand(std::string_view id, EditorContext& context);
    bool setActiveViewportTool(std::string_view id, EditorContext& context);
    [[nodiscard]] IViewportTool* activeViewportTool();
    [[nodiscard]] const IViewportTool* activeViewportTool() const;

    void clear();

    [[nodiscard]] EditorMenuVisibility& menuVisibility() { return menuVisibility_; }
    [[nodiscard]] const EditorMenuVisibility& menuVisibility() const { return menuVisibility_; }

    [[nodiscard]] ViewportRenderSettings& viewportRenderSettings() { return viewportRenderSettings_; }
    [[nodiscard]] const ViewportRenderSettings& viewportRenderSettings() const { return viewportRenderSettings_; }
    [[nodiscard]] ViewportNavigationSettings& viewportNavigationSettings() { return viewportNavigationSettings_; }
    [[nodiscard]] const ViewportNavigationSettings& viewportNavigationSettings() const { return viewportNavigationSettings_; }

    [[nodiscard]] std::vector<RegisteredMainMenuPanel>& mainMenuPanels() { return mainMenuPanels_; }
    [[nodiscard]] std::vector<RegisteredHierarchyPanel>& hierarchyPanels() { return hierarchyPanels_; }
    [[nodiscard]] std::vector<RegisteredFloatingWindow>& floatingWindows() { return floatingWindows_; }
    [[nodiscard]] std::vector<RegisteredStatusBarWidget>& statusBarWidgets() { return statusBarWidgets_; }
    [[nodiscard]] std::vector<RegisteredMenuBarContributor>& menuBarContributors() { return menuBarContributors_; }
    [[nodiscard]] std::vector<std::unique_ptr<IEditorWindow>>& windows() { return windows_; }
    [[nodiscard]] std::vector<std::unique_ptr<IViewportOverlay>>& overlays() { return overlays_; }
    [[nodiscard]] std::vector<EditorCommand>& commands() { return commands_; }
    [[nodiscard]] std::vector<std::unique_ptr<IEditorCommand>>& commandObjects() { return commandObjects_; }
    [[nodiscard]] std::vector<EditorMenuItem>& menuItems() { return menuItems_; }
    [[nodiscard]] std::vector<StatusBarItem>& statusBarItems() { return statusBarItems_; }
    [[nodiscard]] std::vector<std::unique_ptr<IHierarchyProvider>>& hierarchyProviders() { return hierarchyProviders_; }
    [[nodiscard]] std::vector<std::unique_ptr<ISceneProvider>>& sceneProviders() { return sceneProviders_; }
    [[nodiscard]] std::vector<std::unique_ptr<IViewportSceneLayer>>& viewportSceneLayers() { return viewportSceneLayers_; }
    [[nodiscard]] std::vector<std::unique_ptr<IViewportTool>>& viewportTools() { return viewportTools_; }
    [[nodiscard]] std::vector<std::unique_ptr<IPropertiesPanel>>& propertiesPanels() { return propertiesPanels_; }
    [[nodiscard]] std::vector<std::unique_ptr<IContextMenuProvider>>& contextMenuProviders() { return contextMenuProviders_; }

    [[nodiscard]] const std::vector<RegisteredMainMenuPanel>& mainMenuPanels() const { return mainMenuPanels_; }
    [[nodiscard]] const std::vector<RegisteredHierarchyPanel>& hierarchyPanels() const { return hierarchyPanels_; }
    [[nodiscard]] const std::vector<RegisteredFloatingWindow>& floatingWindows() const { return floatingWindows_; }
    [[nodiscard]] const std::vector<RegisteredStatusBarWidget>& statusBarWidgets() const { return statusBarWidgets_; }
    [[nodiscard]] const std::vector<RegisteredMenuBarContributor>& menuBarContributors() const { return menuBarContributors_; }
    [[nodiscard]] const std::vector<std::unique_ptr<IEditorWindow>>& windows() const { return windows_; }
    [[nodiscard]] const std::vector<std::unique_ptr<IViewportOverlay>>& overlays() const { return overlays_; }
    [[nodiscard]] const std::vector<EditorCommand>& commands() const { return commands_; }
    [[nodiscard]] const std::vector<std::unique_ptr<IEditorCommand>>& commandObjects() const { return commandObjects_; }
    [[nodiscard]] const std::vector<EditorMenuItem>& menuItems() const { return menuItems_; }
    [[nodiscard]] const std::vector<StatusBarItem>& statusBarItems() const { return statusBarItems_; }
    [[nodiscard]] const std::vector<std::unique_ptr<IHierarchyProvider>>& hierarchyProviders() const { return hierarchyProviders_; }
    [[nodiscard]] const std::vector<std::unique_ptr<ISceneProvider>>& sceneProviders() const { return sceneProviders_; }
    [[nodiscard]] const std::vector<std::unique_ptr<IViewportSceneLayer>>& viewportSceneLayers() const { return viewportSceneLayers_; }
    [[nodiscard]] const std::vector<std::unique_ptr<IViewportTool>>& viewportTools() const { return viewportTools_; }
    [[nodiscard]] const std::vector<std::unique_ptr<IPropertiesPanel>>& propertiesPanels() const { return propertiesPanels_; }
    [[nodiscard]] const std::vector<std::unique_ptr<IContextMenuProvider>>& contextMenuProviders() const { return contextMenuProviders_; }

private:
    EditorMenuVisibility menuVisibility_;
    ViewportRenderSettings viewportRenderSettings_;
    ViewportNavigationSettings viewportNavigationSettings_;
    std::vector<RegisteredMainMenuPanel> mainMenuPanels_;
    std::vector<RegisteredHierarchyPanel> hierarchyPanels_;
    std::vector<RegisteredFloatingWindow> floatingWindows_;
    std::vector<RegisteredStatusBarWidget> statusBarWidgets_;
    std::vector<RegisteredMenuBarContributor> menuBarContributors_;
    std::vector<std::unique_ptr<IEditorWindow>> windows_;
    std::vector<std::unique_ptr<IViewportOverlay>> overlays_;
    std::vector<EditorCommand> commands_;
    std::vector<std::unique_ptr<IEditorCommand>> commandObjects_;
    std::vector<EditorMenuItem> menuItems_;
    std::vector<StatusBarItem> statusBarItems_;
    std::vector<std::unique_ptr<IHierarchyProvider>> hierarchyProviders_;
    std::vector<std::unique_ptr<ISceneProvider>> sceneProviders_;
    std::vector<std::unique_ptr<IViewportSceneLayer>> viewportSceneLayers_;
    std::vector<std::unique_ptr<IViewportTool>> viewportTools_;
    std::vector<std::unique_ptr<IPropertiesPanel>> propertiesPanels_;
    std::vector<std::unique_ptr<IContextMenuProvider>> contextMenuProviders_;
    int activeViewportToolIndex_ = -1;
};

} // namespace hephaiston
