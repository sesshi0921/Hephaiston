#include "hephaiston/native_dialog/NativeFolderDialog.h"

#include <array>
#include <cstdio>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#endif

namespace hephaiston::native_dialog {
namespace {
std::string readPipe(const char* command) {
    std::array<char, 512> buffer {};
    std::string result;
    FILE* pipe = popen(command, "r");
    if (!pipe) return {};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) result += buffer.data();
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    return result;
}
} // namespace

FolderDialogResult selectFolder(const std::filesystem::path&) {
#if defined(__APPLE__)
    // AppleScript asks NSOpenPanel to select an existing directory, which is a
    // native macOS dialog and does not add a runtime framework dependency.
    const std::string selected = readPipe("osascript -e 'POSIX path of (choose folder with prompt \"Select export folder\")' 2>/dev/null");
    if (selected.empty()) return {};
    const std::filesystem::path path(selected);
    if (!std::filesystem::is_directory(path)) return {{}, "The selected path is not a directory."};
    return {path, {}};
#elif defined(_WIN32)
    BROWSEINFOA info {};
    info.lpszTitle = "Select export folder";
    PIDLIST_ABSOLUTE item = SHBrowseForFolderA(&info);
    if (!item) return {};
    char path[MAX_PATH] {};
    const bool ok = SHGetPathFromIDListA(item, path) != FALSE;
    CoTaskMemFree(item);
    return ok ? FolderDialogResult {std::filesystem::path(path), {}} : FolderDialogResult {{}, "Unable to resolve the selected folder."};
#else
    // zenity is deliberately optional: an unavailable command is reported as
    // an error, while user cancellation retains the currently configured path.
    const std::string selected = readPipe("zenity --file-selection --directory --title='Select export folder' 2>/dev/null");
    if (selected.empty()) return {};
    const std::filesystem::path path(selected);
    if (!std::filesystem::is_directory(path)) return {{}, "The selected path is not a directory."};
    return {path, {}};
#endif
}

} // namespace hephaiston::native_dialog
