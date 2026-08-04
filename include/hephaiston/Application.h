#pragma once

#include "hephaiston/EditorShell.h"
#include "hephaiston/ViewportRenderer.h"

#include <memory>
#include <string>

struct GLFWwindow;

namespace hephaiston {

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool initialize();
    void run();

private:
    void shutdown();
    void setupImGuiStyle();

    GLFWwindow* window_ = nullptr;
    std::unique_ptr<EditorShell> editorShell_;
    std::unique_ptr<ViewportRenderer> viewportRenderer_;
    std::string currentWindowTitle_ = "Hephaiston";
};

} // namespace hephaiston
