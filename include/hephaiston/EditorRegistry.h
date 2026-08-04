#pragma once

#include "hephaiston/EditorTypes.h"

#include <memory>
#include <vector>

namespace hephaiston {

class EditorRegistry {
public:
    void registerPanel(std::unique_ptr<IEditorPanel> panel);
    void registerWindow(std::unique_ptr<IEditorWindow> window);
    void registerOverlay(std::unique_ptr<IViewportOverlay> overlay);
    void registerCommand(EditorCommand command);

    [[nodiscard]] std::vector<std::unique_ptr<IEditorPanel>>& panels() { return panels_; }
    [[nodiscard]] std::vector<std::unique_ptr<IEditorWindow>>& windows() { return windows_; }
    [[nodiscard]] std::vector<std::unique_ptr<IViewportOverlay>>& overlays() { return overlays_; }
    [[nodiscard]] std::vector<EditorCommand>& commands() { return commands_; }

    [[nodiscard]] const std::vector<std::unique_ptr<IEditorPanel>>& panels() const { return panels_; }
    [[nodiscard]] const std::vector<std::unique_ptr<IEditorWindow>>& windows() const { return windows_; }
    [[nodiscard]] const std::vector<std::unique_ptr<IViewportOverlay>>& overlays() const { return overlays_; }
    [[nodiscard]] const std::vector<EditorCommand>& commands() const { return commands_; }

private:
    std::vector<std::unique_ptr<IEditorPanel>> panels_;
    std::vector<std::unique_ptr<IEditorWindow>> windows_;
    std::vector<std::unique_ptr<IViewportOverlay>> overlays_;
    std::vector<EditorCommand> commands_;
};

} // namespace hephaiston
