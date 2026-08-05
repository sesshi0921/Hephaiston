#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace hephaiston::native_dialog {

struct FolderDialogResult {
    std::optional<std::filesystem::path> path;
    std::string error;
    [[nodiscard]] bool cancelled() const { return !path.has_value() && error.empty(); }
};

// Opens the platform folder picker. On unsupported Linux desktop setups it
// returns an error rather than launching a shell or silently changing paths.
[[nodiscard]] FolderDialogResult selectFolder(const std::filesystem::path& initialDirectory = {});

} // namespace hephaiston::native_dialog
