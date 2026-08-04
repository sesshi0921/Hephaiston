#pragma once

#include "hephaiston/EditorRegistry.h"
#include "hephaiston/EditorTypes.h"
#include "hephaiston/SceneRegistry.h"
#include "hephaiston/SelectionManager.h"

namespace hephaiston {

// Short-lived facade passed to plugin lifecycle/tool/menu APIs. Plugins should
// not store this object beyond the callback; if long-lived access is necessary,
// store only the specific service pointer/reference whose lifetime is documented.
class EditorContext {
public:
    EditorContext(EditorRegistry& registry,
                  EditorLayoutState& layoutState,
                  ViewportStatus& viewportStatus,
                  ViewportInputState& viewportInput,
                  ViewportVisibleRect& visibleRect,
                  ViewMode& viewMode,
                  SelectionManager& selection,
                  SceneRegistry& scene)
        : registry_(registry),
          layoutState_(layoutState),
          viewportStatus_(viewportStatus),
          viewportInput_(viewportInput),
          visibleRect_(visibleRect),
          viewMode_(viewMode),
          selection_(selection),
          scene_(scene) {}

    [[nodiscard]] EditorRegistry& registry() { return registry_; }
    [[nodiscard]] EditorLayoutState& layoutState() { return layoutState_; }
    [[nodiscard]] ViewportStatus& viewportStatus() { return viewportStatus_; }
    [[nodiscard]] ViewportInputState& viewportInput() { return viewportInput_; }
    [[nodiscard]] ViewportVisibleRect& visibleRect() { return visibleRect_; }
    [[nodiscard]] ViewMode& viewMode() { return viewMode_; }
    [[nodiscard]] SelectionManager& selection() { return selection_; }
    [[nodiscard]] SceneRegistry& scene() { return scene_; }

    [[nodiscard]] const EditorRegistry& registry() const { return registry_; }
    [[nodiscard]] const EditorLayoutState& layoutState() const { return layoutState_; }
    [[nodiscard]] const ViewportStatus& viewportStatus() const { return viewportStatus_; }
    [[nodiscard]] const ViewportInputState& viewportInput() const { return viewportInput_; }
    [[nodiscard]] const ViewportVisibleRect& visibleRect() const { return visibleRect_; }
    [[nodiscard]] ViewMode viewMode() const { return viewMode_; }
    [[nodiscard]] const SelectionManager& selection() const { return selection_; }
    [[nodiscard]] const SceneRegistry& scene() const { return scene_; }

private:
    EditorRegistry& registry_;
    EditorLayoutState& layoutState_;
    ViewportStatus& viewportStatus_;
    ViewportInputState& viewportInput_;
    ViewportVisibleRect& visibleRect_;
    ViewMode& viewMode_;
    SelectionManager& selection_;
    SceneRegistry& scene_;
};

} // namespace hephaiston
