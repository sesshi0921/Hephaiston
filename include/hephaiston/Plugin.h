#pragma once

#include "hephaiston/EditorContext.h"

#include <cstdint>

#if defined(_WIN32)
#define HEPHAISTON_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define HEPHAISTON_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace hephaiston {

inline constexpr std::uint32_t kPluginApiVersion = 1;

struct PluginDescriptor {
    const char* id = "";
    const char* displayName = "";
    const char* vendor = "";
    const char* version = "0.1.0";
    std::uint32_t apiVersion = kPluginApiVersion;
};

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual PluginDescriptor descriptor() const = 0;
    virtual bool onLoad(EditorContext& context) = 0;
    virtual void onUnload(EditorContext& context) = 0;
};

using CreatePluginFn = IPlugin* (*)();
using DestroyPluginFn = void (*)(IPlugin*);

} // namespace hephaiston

// Convenience macro for C++-ABI plugins built with the same compiler/runtime as
// Hephaiston. The C entry points keep creation/destruction inside the module.
#define HEPHAISTON_DECLARE_PLUGIN(PluginType)                                    \
    HEPHAISTON_PLUGIN_EXPORT hephaiston::IPlugin* hephaistonCreatePlugin() {     \
        return new PluginType();                                                 \
    }                                                                            \
    HEPHAISTON_PLUGIN_EXPORT void hephaistonDestroyPlugin(hephaiston::IPlugin* plugin) { \
        delete plugin;                                                           \
    }
