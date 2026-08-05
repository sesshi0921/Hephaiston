#include "hephaiston/PluginManager.h"

#include <algorithm>
#include <sstream>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace hephaiston {

namespace {

bool isDynamicLibrary(const std::filesystem::path& path) {
#if defined(_WIN32)
    return path.extension() == ".dll";
#elif defined(__APPLE__)
    return path.extension() == ".dylib" || path.extension() == ".so";
#else
    return path.extension() == ".so";
#endif
}

std::string errorMessageForPath(const std::filesystem::path& path, const std::string& message) {
    std::ostringstream oss;
    oss << path.string() << ": " << message;
    return oss.str();
}

} // namespace

PluginManager::~PluginManager() {
    for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
        LoadedPlugin& loaded = **it;
        if (loaded.plugin && loaded.destroy) {
            loaded.destroy(loaded.plugin);
            loaded.plugin = nullptr;
        }
    }
    closeLibrariesOnly();
}

void PluginManager::loadAllFromDirectory(EditorContext& context, const std::filesystem::path& directory) {
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        context.logger().debug("[PluginManager] Plugin directory is unavailable; skipping scan: " + directory.string());
        return;
    }

    context.logger().debug("[PluginManager] Scanning plugin directory: " + directory.string());
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || !isDynamicLibrary(entry.path())) {
            continue;
        }
        loadPlugin(context, entry.path());
    }
}

bool PluginManager::loadPlugin(EditorContext& context, const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path) || !isDynamicLibrary(path)) {
        const auto error = errorMessageForPath(path, "not a supported plugin library");
        loadErrors_.push_back(error);
        context.logger().error("[PluginManager] " + error);
        return false;
    }
    const auto normalized = std::filesystem::weakly_canonical(path);
    for (const auto& loaded : plugins_) {
        if (loaded && loaded->plugin && std::filesystem::weakly_canonical(loaded->path) == normalized) {
            const auto error = errorMessageForPath(path, "plugin is already loaded");
            loadErrors_.push_back(error);
            context.logger().warning("[PluginManager] " + error);
            return false;
        }
    }
    context.logger().info("[PluginManager] Loading plugin library: " + normalized.string());
    return loadOne(context, normalized);
}

std::vector<std::filesystem::path> PluginManager::discoverPlugins(const std::filesystem::path& directory) const {
    std::vector<std::filesystem::path> discovered;
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error) || error) {
        return discovered;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            break;
        }
        if (entry.is_regular_file(error) && !error && isDynamicLibrary(entry.path())) {
            discovered.push_back(entry.path());
        }
    }
    std::sort(discovered.begin(), discovered.end());
    return discovered;
}

bool PluginManager::isPluginLoaded(const std::filesystem::path& path) const {
    std::error_code error;
    const auto normalized = std::filesystem::weakly_canonical(path, error);
    if (error) {
        return false;
    }
    return std::any_of(plugins_.begin(), plugins_.end(), [&](const auto& loaded) {
        if (!loaded || !loaded->plugin) {
            return false;
        }
        std::error_code loadedError;
        return std::filesystem::weakly_canonical(loaded->path, loadedError) == normalized && !loadedError;
    });
}

void PluginManager::unloadAll(EditorContext& context) {
    for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
        LoadedPlugin& loaded = **it;
        if (loaded.plugin) {
            context.logger().info("[PluginManager] Calling onUnload for plugin: " + std::string(loaded.plugin->descriptor().displayName));
            loaded.plugin->onUnload(context);
        }
    }

    for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
        LoadedPlugin& loaded = **it;
        if (loaded.plugin && loaded.destroy) {
            context.logger().debug("[PluginManager] Destroying plugin instance: " + loaded.path.string());
            loaded.destroy(loaded.plugin);
            loaded.plugin = nullptr;
        }
    }

    // Keep libraries open until EditorRegistry-owned extension objects are
    // destroyed by EditorShell. Closing a DLL before deleting plugin-created UI
    // objects would leave virtual tables pointing into an unloaded module.
    descriptors_.clear();
    context.logger().debug("[PluginManager] Plugin descriptors cleared; libraries remain open until registry objects are released.");
}

void PluginManager::releaseUnloadedLibraries() {
    for (auto& loaded : plugins_) {
        if (!loaded || loaded->plugin || !loaded->library) continue;
#if defined(_WIN32)
        FreeLibrary(reinterpret_cast<HMODULE>(loaded->library));
#else
        dlclose(loaded->library);
#endif
        loaded->library = nullptr;
    }
    std::erase_if(plugins_, [](const auto& loaded) { return !loaded || (!loaded->plugin && !loaded->library); });
}

bool PluginManager::loadOne(EditorContext& context, const std::filesystem::path& path) {
    auto loaded = std::make_unique<LoadedPlugin>();
    loaded->path = path;

#if defined(_WIN32)
    loaded->library = reinterpret_cast<void*>(LoadLibraryA(path.string().c_str()));
    if (!loaded->library) {
        const auto error = errorMessageForPath(path, "LoadLibrary failed");
        loadErrors_.push_back(error);
        context.logger().error("[PluginManager] " + error);
        return false;
    }
    auto create = reinterpret_cast<CreatePluginFn>(GetProcAddress(reinterpret_cast<HMODULE>(loaded->library), "hephaistonCreatePlugin"));
    loaded->destroy = reinterpret_cast<DestroyPluginFn>(GetProcAddress(reinterpret_cast<HMODULE>(loaded->library), "hephaistonDestroyPlugin"));
#else
    loaded->library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!loaded->library) {
        const auto error = errorMessageForPath(path, dlerror() ? dlerror() : "dlopen failed");
        loadErrors_.push_back(error);
        context.logger().error("[PluginManager] " + error);
        return false;
    }
    auto create = reinterpret_cast<CreatePluginFn>(dlsym(loaded->library, "hephaistonCreatePlugin"));
    loaded->destroy = reinterpret_cast<DestroyPluginFn>(dlsym(loaded->library, "hephaistonDestroyPlugin"));
#endif

    if (!create || !loaded->destroy) {
        const auto error = errorMessageForPath(path, "missing hephaistonCreatePlugin/hephaistonDestroyPlugin");
        loadErrors_.push_back(error);
        context.logger().critical("[PluginManager] " + error);
#if defined(_WIN32)
        FreeLibrary(reinterpret_cast<HMODULE>(loaded->library));
#else
        dlclose(loaded->library);
#endif
        loaded->library = nullptr;
        return false;
    }

    loaded->plugin = create();
    if (!loaded->plugin) {
        const auto error = errorMessageForPath(path, "hephaistonCreatePlugin returned null");
        loadErrors_.push_back(error);
        context.logger().critical("[PluginManager] " + error);
#if defined(_WIN32)
        FreeLibrary(reinterpret_cast<HMODULE>(loaded->library));
#else
        dlclose(loaded->library);
#endif
        loaded->library = nullptr;
        return false;
    }

    PluginDescriptor descriptor = loaded->plugin->descriptor();
    if (descriptor.apiVersion != kPluginApiVersion) {
        loaded->destroy(loaded->plugin);
        loaded->plugin = nullptr;
#if defined(_WIN32)
        FreeLibrary(reinterpret_cast<HMODULE>(loaded->library));
#else
        dlclose(loaded->library);
#endif
        const auto error = errorMessageForPath(path, "plugin API version mismatch");
        loadErrors_.push_back(error);
        context.logger().critical("[PluginManager] " + error + "; expected API " + std::to_string(kPluginApiVersion) + ", got " + std::to_string(descriptor.apiVersion));
        return false;
    }

    if (!loaded->plugin->onLoad(context)) {
        loaded->destroy(loaded->plugin);
        loaded->plugin = nullptr;
#if defined(_WIN32)
        FreeLibrary(reinterpret_cast<HMODULE>(loaded->library));
#else
        dlclose(loaded->library);
#endif
        const auto error = errorMessageForPath(path, "plugin onLoad returned false");
        loadErrors_.push_back(error);
        context.logger().error("[PluginManager] " + error);
        return false;
    }

    descriptors_.push_back(descriptor);
    plugins_.push_back(std::move(loaded));
    context.logger().info("[PluginManager] Plugin loaded successfully: " + std::string(descriptor.displayName) + " v" + descriptor.version);
    return true;
}

void PluginManager::closeLibrariesOnly() {
    for (auto& loaded : plugins_) {
        if (!loaded || !loaded->library) {
            continue;
        }
#if defined(_WIN32)
        FreeLibrary(reinterpret_cast<HMODULE>(loaded->library));
#else
        dlclose(loaded->library);
#endif
        loaded->library = nullptr;
    }
}

} // namespace hephaiston
