cmake_minimum_required(VERSION 3.24)
project(Hephaiston VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HEPHAISTON_BUILD_WARNINGS "Enable additional compiler warnings" ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
if(NOT EXISTS ${IMGUI_DIR}/imgui.h)
    message(FATAL_ERROR "Dear ImGui docking branch not found at external/imgui. Clone it with: git clone --depth 1 --branch docking https://github.com/ocornut/imgui.git external/imgui")
endif()

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_executable(hephaiston
    src/main.cpp
    src/Application.cpp
    src/EditorRegistry.cpp
    src/EditorShell.cpp
    src/Framebuffer.cpp
    src/ViewportRenderer.cpp
)
target_include_directories(hephaiston PRIVATE include)
target_link_libraries(hephaiston PRIVATE imgui glfw OpenGL::GL)

if(APPLE)
    target_link_libraries(hephaiston PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

if(HEPHAISTON_BUILD_WARNINGS)
    target_compile_options(hephaiston PRIVATE -Wall -Wextra -Wpedantic)
endif()
