#include "hephaiston/PluginManager.h"

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
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || !isDynamicLibrary(entry.path())) {
            continue;
        }
        loadOne(context, entry.path());
    }
}

void PluginManager::unloadAll(EditorContext& context) {
    for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
        LoadedPlugin& loaded = **it;
        if (loaded.plugin) {
            loaded.plugin->onUnload(context);
        }
    }

    for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
        LoadedPlugin& loaded = **it;
        if (loaded.plugin && loaded.destroy) {
            loaded.destroy(loaded.plugin);
            loaded.plugin = nullptr;
        }
    }

    // Keep libraries open until EditorRegistry-owned extension objects are
    // destroyed by EditorShell. Closing a DLL before deleting plugin-created UI
    // objects would leave virtual tables pointing into an unloaded module.
    descriptors_.clear();
}

bool PluginManager::loadOne(EditorContext& context, const std::filesystem::path& path) {
    auto loaded = std::make_unique<LoadedPlugin>();
    loaded->path = path;

#if defined(_WIN32)
    loaded->library = reinterpret_cast<void*>(LoadLibraryA(path.string().c_str()));
    if (!loaded->library) {
        loadErrors_.push_back(errorMessageForPath(path, "LoadLibrary failed"));
        return false;
    }
    auto create = reinterpret_cast<CreatePluginFn>(GetProcAddress(reinterpret_cast<HMODULE>(loaded->library), "hephaistonCreatePlugin"));
    loaded->destroy = reinterpret_cast<DestroyPluginFn>(GetProcAddress(reinterpret_cast<HMODULE>(loaded->library), "hephaistonDestroyPlugin"));
#else
    loaded->library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!loaded->library) {
        loadErrors_.push_back(errorMessageForPath(path, dlerror() ? dlerror() : "dlopen failed"));
        return false;
    }
    auto create = reinterpret_cast<CreatePluginFn>(dlsym(loaded->library, "hephaistonCreatePlugin"));
    loaded->destroy = reinterpret_cast<DestroyPluginFn>(dlsym(loaded->library, "hephaistonDestroyPlugin"));
#endif

    if (!create || !loaded->destroy) {
        loadErrors_.push_back(errorMessageForPath(path, "missing hephaistonCreatePlugin/hephaistonDestroyPlugin"));
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
        loadErrors_.push_back(errorMessageForPath(path, "hephaistonCreatePlugin returned null"));
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
        loadErrors_.push_back(errorMessageForPath(path, "plugin API version mismatch"));
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
        loadErrors_.push_back(errorMessageForPath(path, "plugin onLoad returned false"));
        return false;
    }

    descriptors_.push_back(descriptor);
    plugins_.push_back(std::move(loaded));
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
