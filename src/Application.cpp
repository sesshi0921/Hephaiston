#include "hephaiston/Application.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#endif

#include <chrono>
#include <cstdio>
#include <exception>
#include <thread>
#include <stdexcept>

namespace hephaiston {

Application::Application() = default;

Application::~Application() {
    shutdown();
}

bool Application::initialize() {
    glfwSetErrorCallback([](int error, const char* description) {
        std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
    });

    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(1440, 900, "Hephaiston", nullptr, nullptr);
    if (window_ == nullptr) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    // FPS is capped manually from the editor shell so the cap is independent of monitor refresh rate.
    glfwSwapInterval(0);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "hephaiston_imgui.ini";

    setupImGuiStyle();

    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)) {
        std::fprintf(stderr, "Failed to initialize ImGui GLFW backend\n");
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        std::fprintf(stderr, "Failed to initialize ImGui OpenGL backend\n");
        return false;
    }

    try {
        viewportRenderer_ = std::make_unique<ViewportRenderer>();
        editorShell_ = std::make_unique<EditorShell>();
        editorShell_->initializeCoreRegistry();
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "Initialization error: %s\n", ex.what());
        return false;
    }

    return true;
}

void Application::run() {
    using Clock = std::chrono::steady_clock;

    bool shouldClose = false;
    while (!glfwWindowShouldClose(window_) && !shouldClose) {
        const auto frameStart = Clock::now();

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        editorShell_->draw(*viewportRenderer_, shouldClose);
        const std::string desiredTitle = editorShell_->windowTitle();
        if (desiredTitle != currentWindowTitle_) {
            currentWindowTitle_ = desiredTitle;
            glfwSetWindowTitle(window_, currentWindowTitle_.c_str());
        }

        ImGui::Render();
        int displayW = 0;
        int displayH = 0;
        glfwGetFramebufferSize(window_, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.02f, 0.025f, 0.030f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);

        const int maxFps = editorShell_ != nullptr ? editorShell_->targetMaxFps() : 60;
        if (maxFps > 0) {
            const auto targetFrameTime = std::chrono::duration<double>(1.0 / static_cast<double>(maxFps));
            const auto elapsed = Clock::now() - frameStart;
            if (elapsed < targetFrameTime) {
                std::this_thread::sleep_for(targetFrameTime - elapsed);
            } else {
                std::this_thread::yield();
            }
        } else {
            std::this_thread::yield();
        }
    }
}

void Application::shutdown() {
    viewportRenderer_.reset();
    editorShell_.reset();

    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

void Application::setupImGuiStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8.0f, 6.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.115f, 0.13f, 0.94f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.09f, 0.105f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.32f, 0.46f, 0.70f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.42f, 0.62f, 0.90f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.50f, 0.72f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.17f, 0.26f, 0.36f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.38f, 0.52f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.54f, 0.74f, 1.00f);
}

} // namespace hephaiston
