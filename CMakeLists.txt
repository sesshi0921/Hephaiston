cmake_minimum_required(VERSION 3.24)
project(Hephaiston VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HEPHAISTON_BUILD_WARNINGS "Enable additional compiler warnings" ON)
option(HEPHAISTON_BUILD_BUNDLED_PLUGINS "Build bundled plugins as part of the default ALL target" OFF)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

# cpp-logger is intentionally retained as a Git submodule.  Its optional
# Python bindings and own test suite are disabled for the editor build; Core
# owns the single logger instance and exposes it to DLL plugins through
# EditorContext::logger().
set(CPP_LOGGER_BUILD_PYTHON OFF CACHE BOOL "Build cpp-logger Python bindings" FORCE)
set(CPP_LOGGER_BUILD_TESTS OFF CACHE BOOL "Build cpp-logger tests" FORCE)
add_subdirectory(external/cpp-logger EXCLUDE_FROM_ALL)

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
        target_link_options(${target} PRIVATE "-Wl,-undefined,dynamic_lookup")
    endif()
endfunction()

add_subdirectory(modules/geospatial)
add_subdirectory(modules/geometry)
add_subdirectory(modules/native_dialog)
add_subdirectory(modules/earth)
add_subdirectory(modules/planar_map)

# Convenience include paths for plugin subdirectories. This lets sources under
# plugins/ include API headers directly, e.g. #include "EditorRegistry.h".
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hephaiston
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

set(HEPHAISTON_PLATFORM_INPUT_SOURCE src/PlatformTrackpadGestures.cpp)
if(APPLE)
    enable_language(OBJCXX)
    set(HEPHAISTON_PLATFORM_INPUT_SOURCE src/MacTrackpadGestures.mm)
endif()

add_executable(hephaiston
    src/main.cpp
    src/Application.cpp
    src/EditorLogger.cpp
    src/EditorRegistry.cpp
    src/EditorShell.cpp
    src/Framebuffer.cpp
    src/PluginManager.cpp
    ${HEPHAISTON_PLATFORM_INPUT_SOURCE}
    src/ViewportRenderer.cpp
)
target_include_directories(hephaiston PRIVATE include)
target_include_directories(hephaiston PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/stb)
target_link_libraries(hephaiston PRIVATE hephaiston_plugin_api imgui glfw OpenGL::GL cpp_logger::cpp_logger ${CMAKE_DL_LIBS})
target_compile_definitions(hephaiston PRIVATE HEPHAISTON_LOG_DIRECTORY="${CMAKE_BINARY_DIR}/logs")

if(UNIX AND NOT APPLE)
    target_link_options(hephaiston PRIVATE -rdynamic)
endif()


if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/plugins/CMakeLists.txt)
    # Plugins are independently buildable MODULE targets.  Keep them outside
    # Core's default ALL target so Core and each DLL have distinct build units.
    # Use --target <plugin> or enable HEPHAISTON_BUILD_BUNDLED_PLUGINS when a
    # single command should build everything.
    if(HEPHAISTON_BUILD_BUNDLED_PLUGINS)
        add_subdirectory(plugins)
    else()
        add_subdirectory(plugins EXCLUDE_FROM_ALL)
    endif()
endif()

if(BUILD_TESTING AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
endif()

if(APPLE)
    target_link_libraries(hephaiston PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

if(HEPHAISTON_BUILD_WARNINGS)
    target_compile_options(hephaiston PRIVATE -Wall -Wextra -Wpedantic)
endif()
