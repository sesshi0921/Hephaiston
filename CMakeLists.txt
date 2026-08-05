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

add_library(hephaiston_plugin_api INTERFACE)
target_compile_features(hephaiston_plugin_api INTERFACE cxx_std_20)
target_include_directories(hephaiston_plugin_api INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hephaiston
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(hephaiston_plugin_api INTERFACE imgui)


function(hephaiston_add_plugin target)
    add_library(${target} MODULE ${ARGN})
    target_link_libraries(${target} PRIVATE hephaiston_plugin_api)
    set_target_properties(${target} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    )
    if(APPLE)
        set_target_properties(${target} PROPERTIES BUNDLE FALSE PREFIX "")
    endif()
endfunction()

# Convenience include paths for plugin subdirectories. This lets sources under
# plugins/ include API headers directly, e.g. #include "EditorRegistry.h".
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hephaiston
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

add_executable(hephaiston
    src/main.cpp
    src/Application.cpp
    src/EditorRegistry.cpp
    src/EditorShell.cpp
    src/Framebuffer.cpp
    src/PluginManager.cpp
    src/ViewportRenderer.cpp
)
target_include_directories(hephaiston PRIVATE include)
target_link_libraries(hephaiston PRIVATE hephaiston_plugin_api imgui glfw OpenGL::GL ${CMAKE_DL_LIBS})


if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/plugins/CMakeLists.txt)
    add_subdirectory(plugins)
endif()

if(APPLE)
    target_link_libraries(hephaiston PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

if(HEPHAISTON_BUILD_WARNINGS)
    target_compile_options(hephaiston PRIVATE -Wall -Wextra -Wpedantic)
endif()
