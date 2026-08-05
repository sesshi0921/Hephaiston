#include "hephaiston/EditorRegistry.h"

#include "hephaiston/EditorContext.h"

#include <stdexcept>

namespace hephaiston {

void EditorRegistry::registerMainMenuPanel(std::string id, std::string displayName, std::unique_ptr<IMainMenuPanel> panel) {
    if (id.empty() || displayName.empty() || !panel) {
        throw std::invalid_argument("registerMainMenuPanel received invalid panel");
    }
    mainMenuPanels_.push_back({std::move(id), std::move(displayName), std::move(panel)});
}

void EditorRegistry::registerHierarchyPanel(std::string id, std::string displayName, std::unique_ptr<IHierarchyPanel> panel) {
    if (id.empty() || displayName.empty() || !panel) {
        throw std::invalid_argument("registerHierarchyPanel received invalid panel");
    }
    hierarchyPanels_.push_back({std::move(id), std::move(displayName), std::move(panel)});
}

void EditorRegistry::registerFloatingWindow(std::string id, std::string displayName, std::unique_ptr<IFloatingWindow> window, bool open) {
    if (id.empty() || displayName.empty() || !window) {
        throw std::invalid_argument("registerFloatingWindow received invalid window");
    }
    floatingWindows_.push_back({std::move(id), std::move(displayName), open, std::move(window)});
}

void EditorRegistry::registerStatusBarWidget(std::string id, std::string displayName, std::unique_ptr<IStatusBarWidget> widget, bool visible) {
    if (id.empty() || displayName.empty() || !widget) {
        throw std::invalid_argument("registerStatusBarWidget received invalid widget");
    }
    statusBarWidgets_.push_back({std::move(id), std::move(displayName), visible, std::move(widget)});
}

void EditorRegistry::registerMenuBarContributor(std::string id, std::unique_ptr<IMenuBarContributor> contributor) {
    if (id.empty() || !contributor) {
        throw std::invalid_argument("registerMenuBarContributor received invalid contributor");
    }
    menuBarContributors_.push_back({std::move(id), std::move(contributor)});
}

void EditorRegistry::registerPanel(std::unique_ptr<IEditorPanel> panel) {
    if (!panel) {
        throw std::invalid_argument("registerPanel received null panel");
    }
    const std::string id = panel->id();
    const std::string displayName = panel->displayName();
    registerMainMenuPanel(id, displayName, std::move(panel));
}

void EditorRegistry::registerWindow(std::unique_ptr<IEditorWindow> window) {
    if (!window) {
        throw std::invalid_argument("registerWindow received null window");
    }
    windows_.push_back(std::move(window));
}

void EditorRegistry::registerOverlay(std::unique_ptr<IViewportOverlay> overlay) {
    if (!overlay) {
        throw std::invalid_argument("registerOverlay received null overlay");
    }
    overlays_.push_back(std::move(overlay));
}

void EditorRegistry::registerCommand(EditorCommand command) {
    if (command.id.empty() || !command.execute) {
        throw std::invalid_argument("registerCommand received invalid command");
    }
    commands_.push_back(std::move(command));
}

void EditorRegistry::registerCommand(std::unique_ptr<IEditorCommand> command) {
    if (!command || std::string_view(command->id()).empty()) {
        throw std::invalid_argument("registerCommand received invalid command object");
    }
    commandObjects_.push_back(std::move(command));
}

void EditorRegistry::registerMenuItem(EditorMenuItem item) {
    if (item.id.empty() || item.menuName.empty() || item.label.empty() || !item.execute) {
        throw std::invalid_argument("registerMenuItem received invalid item");
    }
    menuItems_.push_back(std::move(item));
}

void EditorRegistry::registerStatusBarItem(StatusBarItem item) {
    if (item.id.empty()) {
        throw std::invalid_argument("registerStatusBarItem received invalid item");
    }
    statusBarItems_.push_back(std::move(item));
}

void EditorRegistry::registerHierarchyProvider(std::unique_ptr<IHierarchyProvider> provider) {
    if (!provider) {
        throw std::invalid_argument("registerHierarchyProvider received null provider");
    }
    hierarchyProviders_.push_back(std::move(provider));
}

void EditorRegistry::registerSceneProvider(std::unique_ptr<ISceneProvider> provider) {
    if (!provider) {
        throw std::invalid_argument("registerSceneProvider received null provider");
    }
    sceneProviders_.push_back(std::move(provider));
}

void EditorRegistry::registerViewportSceneLayer(std::unique_ptr<IViewportSceneLayer> layer) {
    if (!layer) {
        throw std::invalid_argument("registerViewportSceneLayer received null layer");
    }
    viewportSceneLayers_.push_back(std::move(layer));
}

void EditorRegistry::registerViewportTool(std::unique_ptr<IViewportTool> tool) {
    if (!tool || std::string_view(tool->id()).empty()) {
        throw std::invalid_argument("registerViewportTool received invalid tool");
    }
    if (activeViewportToolIndex_ < 0) {
        activeViewportToolIndex_ = static_cast<int>(viewportTools_.size());
    }
    viewportTools_.push_back(std::move(tool));
}

void EditorRegistry::registerPropertiesPanel(std::unique_ptr<IPropertiesPanel> panel) {
    if (!panel || std::string_view(panel->id()).empty()) {
        throw std::invalid_argument("registerPropertiesPanel received invalid panel");
    }
    propertiesPanels_.push_back(std::move(panel));
}

void EditorRegistry::registerContextMenuProvider(std::unique_ptr<IContextMenuProvider> provider) {
    if (!provider || std::string_view(provider->id()).empty()) {
        throw std::invalid_argument("registerContextMenuProvider received invalid provider");
    }
    contextMenuProviders_.push_back(std::move(provider));
}

bool EditorRegistry::executeCommand(std::string_view id, EditorContext& context) {
    for (auto& command : commandObjects_) {
        if (command && std::string_view(command->id()) == id && command->canExecute(context)) {
            command->execute(context);
            return true;
        }
    }
    for (auto& command : commands_) {
        if (command.id == id && command.execute) {
            command.execute();
            return true;
        }
    }
    return false;
}

bool EditorRegistry::setActiveViewportTool(std::string_view id, EditorContext& context) {
    for (int i = 0; i < static_cast<int>(viewportTools_.size()); ++i) {
        if (viewportTools_[i] && viewportTools_[i]->id() == id) {
            if (activeViewportToolIndex_ == i) {
                return true;
            }
            if (IViewportTool* active = activeViewportTool()) {
                active->onDeactivated(context);
            }
            activeViewportToolIndex_ = i;
            viewportTools_[i]->onActivated(context);
            return true;
        }
    }
    return false;
}

IViewportTool* EditorRegistry::activeViewportTool() {
    if (activeViewportToolIndex_ < 0 || activeViewportToolIndex_ >= static_cast<int>(viewportTools_.size())) {
        return nullptr;
    }
    return viewportTools_[activeViewportToolIndex_].get();
}

const IViewportTool* EditorRegistry::activeViewportTool() const {
    if (activeViewportToolIndex_ < 0 || activeViewportToolIndex_ >= static_cast<int>(viewportTools_.size())) {
        return nullptr;
    }
    return viewportTools_[activeViewportToolIndex_].get();
}

void EditorRegistry::clear() {
    mainMenuPanels_.clear();
    hierarchyPanels_.clear();
    floatingWindows_.clear();
    statusBarWidgets_.clear();
    menuBarContributors_.clear();
    windows_.clear();
    overlays_.clear();
    commands_.clear();
    commandObjects_.clear();
    menuItems_.clear();
    statusBarItems_.clear();
    hierarchyProviders_.clear();
    sceneProviders_.clear();
    viewportSceneLayers_.clear();
    viewportTools_.clear();
    propertiesPanels_.clear();
    contextMenuProviders_.clear();
    activeViewportToolIndex_ = -1;
}

} // namespace hephaiston
