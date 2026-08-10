#pragma once

#include "EditorBuffer.hpp"
#include "LineNumberGutter.hpp"
#include "TypingMetrics.hpp"

#include <gtk/gtk.h>

#include <filesystem>
#include <string>

class Application {
public:
    int Run(int argc, char** argv);

private:
    enum class Mode {
        Normal,
        Insert,
        Command,
        Search
    };

    static void OnActivate(GtkApplication* app, gpointer userData);
    static gboolean OnTextKeyPress(GtkWidget* widget, GdkEventKey* event, gpointer userData);
    static gboolean OnCommandEntryKeyPress(GtkWidget* widget, GdkEventKey* event, gpointer userData);
    static void OnBufferChanged(GtkTextBuffer* buffer, gpointer userData);
    static void OnOpenClicked(GtkButton* button, gpointer userData);
    static void OnSaveClicked(GtkButton* button, gpointer userData);
    static void OnSaveAsClicked(GtkButton* button, gpointer userData);
    static void OnNewClicked(GtkButton* button, gpointer userData);
    static gboolean OnWindowDelete(GtkWidget* widget, GdkEvent* event, gpointer userData);
    static gboolean OnRefreshTimer(gpointer userData);

    void BuildWindow(GtkApplication* app);

    gboolean HandleTextKeyPress(GdkEventKey* event);
    gboolean HandleNormalKey(GdkEventKey* event);
    gboolean HandleInsertKey(GdkEventKey* event);
    gboolean HandleCommandEntryKey(GdkEventKey* event);

    void EnterMode(Mode mode);
    void ExecuteCommand(const std::string& command);

    bool LoadFile(const std::filesystem::path& path, bool force);
    bool SaveFile(const std::filesystem::path& path = {});
    void NewBuffer(bool force);
    void OpenFileDialog();
    bool SaveFileDialog();
    bool ConfirmDiscardOrSave();

    void SyncGtkFromEditorBuffer();
    void SyncEditorBufferFromGtk();

    GtkTextIter CursorIter() const;
    void PlaceCursor(GtkTextIter iter, bool scroll = true);
    void MoveHorizontal(int direction);
    void MoveVertical(int direction);
    void MoveWordForward();
    void MoveWordBackward();
    void MoveLineStart();
    void MoveLineEnd(bool insertionPoint);
    void MoveDocumentStart();
    void MoveDocumentEnd();
    void DeleteCharacter();
    void DeleteLine();
    void OpenLineBelow();
    void OpenLineAbove();

    bool FindNext(const std::string& query, bool wrap);

    void UpdateStatusBar();
    void UpdateWindowTitle();
    void SetStatus(std::string status);
    std::string ModeName() const;

    static std::string Trim(std::string value);
    static std::string Unquote(std::string value);

    GtkApplication* app_{nullptr};
    GtkWidget* window_{nullptr};
    GtkWidget* textView_{nullptr};
    GtkTextBuffer* textBuffer_{nullptr};
    GtkWidget* statusLabel_{nullptr};
    GtkWidget* metricsLabel_{nullptr};
    GtkWidget* commandBox_{nullptr};
    GtkWidget* commandPromptLabel_{nullptr};
    GtkWidget* commandEntry_{nullptr};

    EditorBuffer editorBuffer_;
    TypingMetrics typingMetrics_;
    LineNumberGutter lineNumberGutter_;

    Mode mode_{Mode::Normal};
    bool pendingDelete_{false};
    bool pendingGoto_{false};
    bool suppressBufferChanged_{false};

    std::filesystem::path initialPath_;
    std::string lastSearch_;
    std::string status_;

    guint refreshTimerId_{0};
};
