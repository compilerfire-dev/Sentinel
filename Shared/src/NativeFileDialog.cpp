#include "NativeFileDialog.hpp"

#include <gtk/gtk.h>

#include <array>
#include <cerrno>
#include <filesystem>
#include <optional>
#include <string>

#if defined(__unix__) || defined(__linux__)
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace SentinelShared {
namespace {

std::optional<std::filesystem::path> RunGtkJsonChooser(
    const std::filesystem::path& currentPath
) {
    int argc = 0;
    char** argv = nullptr;
    if (gtk_init_check(&argc, &argv) == FALSE) return std::nullopt;

    GtkFileChooserNative* dialog = gtk_file_chooser_native_new(
        "Select Sentinel JSON data file",
        nullptr,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Open",
        "_Cancel"
    );

    if (!dialog) return std::nullopt;

    GtkFileFilter* jsonFilter = gtk_file_filter_new();
    gtk_file_filter_set_name(jsonFilter, "JSON files (*.json)");
    gtk_file_filter_add_pattern(jsonFilter, "*.json");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), jsonFilter);

    GtkFileFilter* allFilter = gtk_file_filter_new();
    gtk_file_filter_set_name(allFilter, "All files");
    gtk_file_filter_add_pattern(allFilter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), allFilter);

    if (!currentPath.empty()) {
        std::error_code error;
        const auto absolutePath = std::filesystem::absolute(currentPath, error);
        const auto path = error ? currentPath : absolutePath;

        if (std::filesystem::exists(path, error) && !error) {
            gtk_file_chooser_set_filename(
                GTK_FILE_CHOOSER(dialog),
                path.string().c_str()
            );
        } else {
            const auto parent = path.has_parent_path()
                ? path.parent_path()
                : std::filesystem::path{};

            error.clear();
            if (!parent.empty() &&
                std::filesystem::exists(parent, error) && !error) {
                gtk_file_chooser_set_current_folder(
                    GTK_FILE_CHOOSER(dialog),
                    parent.string().c_str()
                );
            }
        }
    }

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

#if defined(__unix__) || defined(__linux__)

bool WriteAll(int descriptor, const char* data, std::size_t size) {
    while (size > 0) {
        const ssize_t written = ::write(descriptor, data, size);
        if (written < 0) {
            if (errno == EINTR) continue;
            return false;
        }

        data += written;
        size -= static_cast<std::size_t>(written);
    }
    return true;
}

std::optional<std::filesystem::path> RunChooserInChildProcess(
    const std::filesystem::path& currentPath
) {
    int descriptors[2]{};
    if (::pipe(descriptors) != 0) return std::nullopt;

    const pid_t child = ::fork();
    if (child < 0) {
        ::close(descriptors[0]);
        ::close(descriptors[1]);
        return std::nullopt;
    }

    if (child == 0) {
        // GTK is deliberately initialized only in this child. The ncurses
        // parent never enters GTK/GLib's nested event loop, so GTK cannot
        // leave the terminal application's event state in a bad condition.
        ::close(descriptors[0]);

        const auto selected = RunGtkJsonChooser(currentPath);
        int exitCode = 1;

        if (selected) {
            const std::string path = selected->string();
            if (WriteAll(descriptors[1], path.data(), path.size())) {
                exitCode = 0;
            } else {
                exitCode = 2;
            }
        }

        ::close(descriptors[1]);
        _exit(exitCode);
    }

    ::close(descriptors[1]);

    std::string selectedPath;
    std::array<char, 4096> buffer{};

    while (true) {
        const ssize_t count = ::read(
            descriptors[0],
            buffer.data(),
            buffer.size()
        );

        if (count > 0) {
            selectedPath.append(
                buffer.data(),
                static_cast<std::size_t>(count)
            );
            continue;
        }

        if (count < 0 && errno == EINTR) continue;
        break;
    }

    ::close(descriptors[0]);

    int status = 0;
    while (::waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) return std::nullopt;
    }

    // Discard terminal keystrokes/focus escape sequences that may have queued
    // while the desktop chooser had focus. They belong to the dialog session,
    // not to Sentinel's command line.
    if (::isatty(STDIN_FILENO)) {
        ::tcflush(STDIN_FILENO, TCIFLUSH);
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || selectedPath.empty()) {
        return std::nullopt;
    }

    return std::filesystem::path(selectedPath);
}

#endif

} // namespace

std::optional<std::filesystem::path> SelectJsonFile(
    const std::filesystem::path& currentPath
) {
#if defined(__unix__) || defined(__linux__)
    return RunChooserInChildProcess(currentPath);
#else
    return RunGtkJsonChooser(currentPath);
#endif
}

} // namespace SentinelShared
