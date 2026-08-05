#include "hephaiston/EditorLogger.h"

#include <filesystem>
#include <stdexcept>
#include <string>

int main() {
    const auto logFile = std::filesystem::path(HEPHAISTON_LOG_DIRECTORY) / "cpp_logger.log";

    {
        auto logger = hephaiston::createEditorLogger();
        logger->info("[EditorLoggerTests] Shared Core logger smoke test.");
        logger->warning("[EditorLoggerTests] Warning-level flush smoke test.");
    }

    std::error_code error;
    if (!std::filesystem::exists(logFile, error) || error || std::filesystem::file_size(logFile, error) == 0 || error) {
        throw std::runtime_error("cpp_logger did not flush the shared Core log file");
    }
    return 0;
}
