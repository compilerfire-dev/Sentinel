#include "NativeFileDialog.hpp"

#include <gtk/gtk.h>

#include <filesystem>
#include <string>

namespace SentinelShared {
namespace {

bool EnsureGtkInitialized() {
    static bool attempted = false;
    static bool available = false;

    if (!attempted) {
        attempted = true;
        int argc = 0;
        char** argv = nullptr;
        available = gtk_init_check(&argc, &argv) != FALSE;
    }

    return available;
}

void SetInitialLocation(GtkFileChooser* chooser, const std::filesystem::path& currentPath) {
    if (!chooser || currentPath.empty()) return;

    std::error_code error;
    const auto absolutePath = std::filesystem::absolute(currentPath, error);
    const auto path = error ? currentPath : absolutePath;

    if (std::filesystem::exists(path, error) && !error) {
        gtk_file_chooser_set_filename(chooser, path.string().c_str());
        return;
    }

    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path{};
    if (!parent.empty() && std::filesystem::exists(parent, error) && !error) {
        gtk_file_chooser_set_current_folder(chooser, parent.string().c_str());
    }
}

} // namespace

std::optional<std::filesystem::path> SelectJsonFile(
    const std::filesystem::path& currentPath
) {
    if (!EnsureGtkInitialized()) return std::nullopt;

    GtkFileChooserNative* dialog = gtk_file_chooser_native_new(
        "Select Sentinel JSON data file",
        nullptr,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Open",
        "_Cancel"
    );

    GtkFileFilter* jsonFilter = gtk_file_filter_new();
    gtk_file_filter_set_name(jsonFilter, "JSON files (*.json)");
    gtk_file_filter_add_pattern(jsonFilter, "*.json");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), jsonFilter);

    GtkFileFilter* allFilter = gtk_file_filter_new();
    gtk_file_filter_set_name(allFilter, "All files");
    gtk_file_filter_add_pattern(allFilter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), allFilter);

    SetInitialLocation(GTK_FILE_CHOOSER(dialog), currentPath);

    std::optional<std::filesystem::path> selected;
    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            selected = std::filesystem::path(filename);
            g_free(filename);
        }
    }

    g_object_unref(dialog);
    return selected;
}

} // namespace SentinelShared
