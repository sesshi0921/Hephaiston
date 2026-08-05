#include "hephaiston/EditorLogger.h"

#include <cpp_logger/logger.hpp>

#include <chrono>
#include <string>
#include <thread>

namespace hephaiston {

namespace {

#ifndef HEPHAISTON_LOG_DIRECTORY
#define HEPHAISTON_LOG_DIRECTORY "build/logs"
#endif

cpp_logger::LogLevel toCppLoggerLevel(EditorLogLevel level) {
    switch (level) {
    case EditorLogLevel::Trace: return cpp_logger::LogLevel::Trace;
    case EditorLogLevel::Debug: return cpp_logger::LogLevel::Debug;
    case EditorLogLevel::Info: return cpp_logger::LogLevel::Info;
    case EditorLogLevel::Warning: return cpp_logger::LogLevel::Warning;
    case EditorLogLevel::Error: return cpp_logger::LogLevel::Error;
    case EditorLogLevel::Critical: return cpp_logger::LogLevel::Critical;
    }
    return cpp_logger::LogLevel::Info;
}

class CppEditorLogger final : public IEditorLogger {
public:
    CppEditorLogger()
        : logger_(cpp_logger::LogLevel::Debug, 16, HEPHAISTON_LOG_DIRECTORY) {
        logger_.info("[Core] cpp_logger initialized; minimum level is Debug; folder=" HEPHAISTON_LOG_DIRECTORY);
    }

    ~CppEditorLogger() override {
        // cpp_logger uses an asynchronous backend.  Its public API does not
        // expose a synchronous flush, so a final critical entry plus a short
        // drain window ensures normal Info/Warning diagnostics survive a
        // graceful editor shutdown as well.
        logger_.critical("[Core] Logger shutdown; flushing pending records.");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void log(EditorLogLevel level, std::string_view message) override {
        logger_.log(toCppLoggerLevel(level), std::string(message));
    }
    void trace(std::string_view message) override { log(EditorLogLevel::Trace, message); }
    void debug(std::string_view message) override { log(EditorLogLevel::Debug, message); }
    void info(std::string_view message) override { log(EditorLogLevel::Info, message); }
    void warning(std::string_view message) override { log(EditorLogLevel::Warning, message); }
    void error(std::string_view message) override { log(EditorLogLevel::Error, message); }
    void critical(std::string_view message) override { log(EditorLogLevel::Critical, message); }

private:
    cpp_logger::Logger logger_;
};

} // namespace

std::unique_ptr<IEditorLogger> createEditorLogger() {
    return std::make_unique<CppEditorLogger>();
}

} // namespace hephaiston
