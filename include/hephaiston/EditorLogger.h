#pragma once

#include <memory>
#include <string_view>

namespace hephaiston {

// Logger surface made available to Core extensions and dynamically-loaded
// plugins.  The implementation remains owned by Core so every component writes
// through the same asynchronously-flushed cpp_logger instance.
enum class EditorLogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
};

class IEditorLogger {
public:
    virtual ~IEditorLogger() = default;

    virtual void log(EditorLogLevel level, std::string_view message) = 0;
    virtual void trace(std::string_view message) = 0;
    virtual void debug(std::string_view message) = 0;
    virtual void info(std::string_view message) = 0;
    virtual void warning(std::string_view message) = 0;
    virtual void error(std::string_view message) = 0;
    virtual void critical(std::string_view message) = 0;
};

// Creates Core's cpp_logger-backed, process-wide editor logger.  Plugins must
// obtain logging through EditorContext::logger(), not create their own logger.
[[nodiscard]] std::unique_ptr<IEditorLogger> createEditorLogger();

} // namespace hephaiston
