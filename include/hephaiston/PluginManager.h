#pragma once

#include "hephaiston/Plugin.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace hephaiston {

class PluginManager {
public:
    PluginManager() = default;
    ~PluginManager();

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    void loadAllFromDirectory(EditorContext& context, const std::filesystem::path& directory);
    void unloadAll(EditorContext& context);

    [[nodiscard]] const std::vector<PluginDescriptor>& loadedPlugins() const { return descriptors_; }
    [[nodiscard]] const std::vector<std::string>& loadErrors() const { return loadErrors_; }

private:
    struct LoadedPlugin {
        std::filesystem::path path;
        void* library = nullptr;
        IPlugin* plugin = nullptr;
        DestroyPluginFn destroy = nullptr;
    };

    bool loadOne(EditorContext& context, const std::filesystem::path& path);
    void closeLibrariesOnly();

    std::vector<std::unique_ptr<LoadedPlugin>> plugins_;
    std::vector<PluginDescriptor> descriptors_;
    std::vector<std::string> loadErrors_;
};

} // namespace hephaiston
