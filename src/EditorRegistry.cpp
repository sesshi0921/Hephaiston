#include "hephaiston/EditorRegistry.h"

#include <stdexcept>

namespace hephaiston {

void EditorRegistry::registerPanel(std::unique_ptr<IEditorPanel> panel) {
    if (!panel) {
        throw std::invalid_argument("registerPanel received null panel");
    }
    panels_.push_back(std::move(panel));
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

} // namespace hephaiston
