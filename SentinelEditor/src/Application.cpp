#include "Application.hpp"

#include <algorithm>
#include <cctype>
#include <gdk/gdkkeysyms.h>
#include <sstream>
#include <utility>

namespace {

constexpr guint RefreshIntervalMilliseconds = 200;

bool HasEditingModifier(GdkModifierType state) {
    return (state & (GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK)) != 0;
}

} // namespace

int Application::Run(int argc, char** argv) {
    if (argc > 1 && argv[1] && *argv[1]) initialPath_ = argv[1];

    app_ = gtk_application_new(
        "dev.compilerfire.sentinel.editor",
        G_APPLICATION_FLAGS_NONE
    );
    g_signal_connect(app_, "activate", G_CALLBACK(OnActivate), this);

    char* gtkArgv[] = {
        (argc > 0 && argv && argv[0]) ? argv[0] : const_cast<char*>("SentinelEditor"),
        nullptr
    };
    const int result = g_application_run(G_APPLICATION(app_), 1, gtkArgv);

    if (refreshTimerId_ != 0) {
        g_source_remove(refreshTimerId_);
        refreshTimerId_ = 0;
    }
    g_object_unref(app_);
    app_ = nullptr;
    return result;
}

void Application::OnActivate(GtkApplication* app, gpointer userData) {
    static_cast<Application*>(userData)->BuildWindow(app);
}

gboolean Application::OnTextKeyPress(GtkWidget*, GdkEventKey* event, gpointer userData) {
    return static_cast<Application*>(userData)->HandleTextKeyPress(event);
}

gboolean Application::OnCommandEntryKeyPress(GtkWidget*, GdkEventKey* event, gpointer userData) {
    return static_cast<Application*>(userData)->HandleCommandEntryKey(event);
}

void Application::OnBufferChanged(GtkTextBuffer* buffer, gpointer userData) {
    auto* self = static_cast<Application*>(userData);
    if (!self->suppressBufferChanged_) gtk_text_buffer_set_modified(buffer, TRUE);
    self->UpdateStatusBar();
    self->UpdateWindowTitle();
}

void Application::OnOpenClicked(GtkButton*, gpointer userData) {
    static_cast<Application*>(userData)->OpenFileDialog();
}

void Application::OnSaveClicked(GtkButton*, gpointer userData) {
    static_cast<Application*>(userData)->SaveFile();
}

void Application::OnSaveAsClicked(GtkButton*, gpointer userData) {
    static_cast<Application*>(userData)->SaveFileDialog();
}

void Application::OnNewClicked(GtkButton*, gpointer userData) {
    static_cast<Application*>(userData)->NewBuffer(false);
}

gboolean Application::OnWindowDelete(GtkWidget*, GdkEvent*, gpointer userData) {
    auto* self = static_cast<Application*>(userData);
    return self->ConfirmDiscardOrSave() ? FALSE : TRUE;
}

gboolean Application::OnRefreshTimer(gpointer userData) {
    auto* self = static_cast<Application*>(userData);
    if (!self->window_) return G_SOURCE_REMOVE;
    self->UpdateStatusBar();
    return G_SOURCE_CONTINUE;
}

void Application::BuildWindow(GtkApplication* app) {
    window_ = gtk_application_window_new(app);
    gtk_window_set_default_size(GTK_WINDOW(window_), 1400, 900);
    g_signal_connect(window_, "delete-event", G_CALLBACK(OnWindowDelete), this);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window_), root);

    GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 8);

    GtkWidget* newButton = gtk_button_new_with_label("New");
    GtkWidget* openButton = gtk_button_new_with_label("Open");
    GtkWidget* saveButton = gtk_button_new_with_label("Save");
    GtkWidget* saveAsButton = gtk_button_new_with_label("Save As");
    GtkWidget* editorLabel = gtk_label_new("SentinelEditor");
    gtk_widget_set_hexpand(editorLabel, TRUE);
    gtk_label_set_xalign(GTK_LABEL(editorLabel), 1.0F);

    gtk_box_pack_start(GTK_BOX(toolbar), newButton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), openButton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), saveButton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), saveAsButton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), editorLabel, TRUE, TRUE, 8);
    gtk_box_pack_start(GTK_BOX(root), toolbar, FALSE, FALSE, 0);

    GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(root), separator, FALSE, FALSE, 0);

    GtkWidget* editorRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(editorRow, TRUE);
    gtk_widget_set_vexpand(editorRow, TRUE);
    gtk_box_pack_start(GTK_BOX(root), editorRow, TRUE, TRUE, 0);

    GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scrolled),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC
    );
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);

    textView_ = gtk_text_view_new();
    textBuffer_ = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textView_));
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(textView_), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textView_), GTK_WRAP_NONE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textView_), 12);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(textView_), 12);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(textView_), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(textView_), 10);
    gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(textView_), TRUE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textView_), TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled), textView_);

    lineNumberGutter_.Attach(
        GTK_TEXT_VIEW(textView_),
        GTK_SCROLLED_WINDOW(scrolled)
    );
    gtk_box_pack_start(
        GTK_BOX(editorRow),
        lineNumberGutter_.Widget(),
        FALSE,
        FALSE,
        0
    );
    gtk_box_pack_start(GTK_BOX(editorRow), scrolled, TRUE, TRUE, 0);

    GtkWidget* statusBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(statusBox), 6);
    statusLabel_ = gtk_label_new("");
    metricsLabel_ = gtk_label_new("");
    gtk_widget_set_hexpand(statusLabel_, TRUE);
    gtk_label_set_xalign(GTK_LABEL(statusLabel_), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(metricsLabel_), 1.0F);
    gtk_box_pack_start(GTK_BOX(statusBox), statusLabel_, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(statusBox), metricsLabel_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root), statusBox, FALSE, FALSE, 0);

    commandBox_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(commandBox_), 6);
    commandPromptLabel_ = gtk_label_new(":");
    commandEntry_ = gtk_entry_new();
    gtk_widget_set_hexpand(commandEntry_, TRUE);
    gtk_box_pack_start(GTK_BOX(commandBox_), commandPromptLabel_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(commandBox_), commandEntry_, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(root), commandBox_, FALSE, FALSE, 0);

    g_signal_connect(textView_, "key-press-event", G_CALLBACK(OnTextKeyPress), this);
    g_signal_connect(commandEntry_, "key-press-event", G_CALLBACK(OnCommandEntryKeyPress), this);
    g_signal_connect(textBuffer_, "changed", G_CALLBACK(OnBufferChanged), this);
    g_signal_connect(newButton, "clicked", G_CALLBACK(OnNewClicked), this);
    g_signal_connect(openButton, "clicked", G_CALLBACK(OnOpenClicked), this);
    g_signal_connect(saveButton, "clicked", G_CALLBACK(OnSaveClicked), this);
    g_signal_connect(saveAsButton, "clicked", G_CALLBACK(OnSaveAsClicked), this);

    SyncGtkFromEditorBuffer();
    if (!initialPath_.empty()) LoadFile(initialPath_, true);
    else SetStatus("Ready. Press i for Insert mode, : for commands, / to search.");

    refreshTimerId_ = g_timeout_add(
        RefreshIntervalMilliseconds,
        OnRefreshTimer,
        this
    );

    gtk_widget_show_all(window_);
    EnterMode(Mode::Normal);
    UpdateWindowTitle();
    UpdateStatusBar();
}

gboolean Application::HandleTextKeyPress(GdkEventKey* event) {
    if (!event) return FALSE;
    if (mode_ == Mode::Insert) return HandleInsertKey(event);
    if (mode_ == Mode::Normal) return HandleNormalKey(event);
    return TRUE;
}

gboolean Application::HandleInsertKey(GdkEventKey* event) {
    if (event->keyval == GDK_KEY_Escape) {
        EnterMode(Mode::Normal);
        return TRUE;
    }

    if ((event->state & GDK_CONTROL_MASK) != 0 &&
        (event->keyval == GDK_KEY_s || event->keyval == GDK_KEY_S)) {
        SaveFile();
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        typingMetrics_.RecordLine();
        return FALSE;
    }

    if (event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_ISO_Left_Tab) {
        typingMetrics_.RecordCharacter();
        return FALSE;
    }

    if (!HasEditingModifier(static_cast<GdkModifierType>(event->state))) {
        const gunichar character = gdk_keyval_to_unicode(event->keyval);
        if (character >= 0x20U && character != 0x7FU) {
            typingMetrics_.RecordCharacter();
        }
    }

    return FALSE;
}

gboolean Application::HandleNormalKey(GdkEventKey* event) {
    const guint key = event->keyval;

    if (pendingDelete_) {
        pendingDelete_ = false;
        if (key == GDK_KEY_d) DeleteLine();
        else SetStatus("Incomplete delete command cancelled.");
        return TRUE;
    }

    if (pendingGoto_) {
        pendingGoto_ = false;
        if (key == GDK_KEY_g) MoveDocumentStart();
        else SetStatus("Incomplete goto command cancelled.");
        return TRUE;
    }

    if ((event->state & GDK_CONTROL_MASK) != 0) {
        if (key == GDK_KEY_s || key == GDK_KEY_S) {
            SaveFile();
            return TRUE;
        }
        if (key == GDK_KEY_b || key == GDK_KEY_B || key == GDK_KEY_Page_Up) {
            for (int index = 0; index < 20; ++index) MoveVertical(-1);
            return TRUE;
        }
        if (key == GDK_KEY_f || key == GDK_KEY_F || key == GDK_KEY_Page_Down) {
            for (int index = 0; index < 20; ++index) MoveVertical(1);
            return TRUE;
        }
    }

    switch (key) {
        case GDK_KEY_Left:
        case GDK_KEY_h:
            MoveHorizontal(-1);
            return TRUE;
        case GDK_KEY_Right:
        case GDK_KEY_l:
            MoveHorizontal(1);
            return TRUE;
        case GDK_KEY_Up:
        case GDK_KEY_k:
            MoveVertical(-1);
            return TRUE;
        case GDK_KEY_Down:
        case GDK_KEY_j:
            MoveVertical(1);
            return TRUE;
        case GDK_KEY_Page_Up:
            for (int index = 0; index < 20; ++index) MoveVertical(-1);
            return TRUE;
        case GDK_KEY_Page_Down:
            for (int index = 0; index < 20; ++index) MoveVertical(1);
            return TRUE;
        case GDK_KEY_0:
        case GDK_KEY_Home:
            MoveLineStart();
            return TRUE;
        case GDK_KEY_dollar:
        case GDK_KEY_End:
            MoveLineEnd(false);
            return TRUE;
        case GDK_KEY_w:
            MoveWordForward();
            return TRUE;
        case GDK_KEY_b:
            MoveWordBackward();
            return TRUE;
        case GDK_KEY_g:
            pendingGoto_ = true;
            SetStatus("g");
            return TRUE;
        case GDK_KEY_G:
            MoveDocumentEnd();
            return TRUE;
        case GDK_KEY_i:
            EnterMode(Mode::Insert);
            return TRUE;
        case GDK_KEY_a: {
            GtkTextIter iter = CursorIter();
            if (!gtk_text_iter_ends_line(&iter)) gtk_text_iter_forward_char(&iter);
            PlaceCursor(iter, false);
            EnterMode(Mode::Insert);
            return TRUE;
        }
        case GDK_KEY_I:
            MoveLineStart();
            EnterMode(Mode::Insert);
            return TRUE;
        case GDK_KEY_A:
            MoveLineEnd(true);
            EnterMode(Mode::Insert);
            return TRUE;
        case GDK_KEY_o:
            OpenLineBelow();
            return TRUE;
        case GDK_KEY_O:
            OpenLineAbove();
            return TRUE;
        case GDK_KEY_x:
            DeleteCharacter();
            return TRUE;
        case GDK_KEY_d:
            pendingDelete_ = true;
            SetStatus("d");
            return TRUE;
        case GDK_KEY_colon:
            EnterMode(Mode::Command);
            return TRUE;
        case GDK_KEY_slash:
            EnterMode(Mode::Search);
            return TRUE;
        case GDK_KEY_n:
            if (lastSearch_.empty()) SetStatus("No previous search.");
            else if (!FindNext(lastSearch_, true)) SetStatus("Pattern not found: " + lastSearch_);
            return TRUE;
        case GDK_KEY_Escape:
            pendingDelete_ = false;
            pendingGoto_ = false;
            SetStatus("");
            return TRUE;
        default:
            return TRUE;
    }
}

gboolean Application::HandleCommandEntryKey(GdkEventKey* event) {
    if (!event) return FALSE;

    if (event->keyval == GDK_KEY_Escape) {
        EnterMode(Mode::Normal);
        return TRUE;
    }

    if (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter) {
        return FALSE;
    }

    const char* raw = gtk_entry_get_text(GTK_ENTRY(commandEntry_));
    const std::string value = raw ? raw : "";
    const Mode previousMode = mode_;
    EnterMode(Mode::Normal);

    if (previousMode == Mode::Command) {
        ExecuteCommand(value);
    } else if (previousMode == Mode::Search) {
        if (!value.empty()) {
            lastSearch_ = value;
            if (!FindNext(value, true)) SetStatus("Pattern not found: " + value);
        }
    }

    return TRUE;
}

void Application::EnterMode(Mode mode) {
    mode_ = mode;
    pendingDelete_ = false;
    pendingGoto_ = false;

    if (!textView_) return;

    if (mode_ == Mode::Insert) {
        gtk_text_view_set_editable(GTK_TEXT_VIEW(textView_), TRUE);
        gtk_widget_hide(commandBox_);
        gtk_widget_grab_focus(textView_);
        SetStatus("INSERT mode");
    } else if (mode_ == Mode::Normal) {
        gtk_text_view_set_editable(GTK_TEXT_VIEW(textView_), FALSE);
        gtk_widget_hide(commandBox_);
        gtk_widget_grab_focus(textView_);
    } else {
        gtk_text_view_set_editable(GTK_TEXT_VIEW(textView_), FALSE);
        gtk_label_set_text(
            GTK_LABEL(commandPromptLabel_),
            mode_ == Mode::Command ? ":" : "/"
        );
        gtk_entry_set_text(GTK_ENTRY(commandEntry_), "");
        gtk_widget_show(commandBox_);
        gtk_widget_grab_focus(commandEntry_);
    }

    UpdateStatusBar();
}

void Application::ExecuteCommand(const std::string& rawCommand) {
    const std::string command = Trim(rawCommand);
    if (command.empty()) return;

    const auto space = command.find_first_of(" \t");
    const std::string operation = space == std::string::npos
        ? command
        : command.substr(0, space);
    const std::string argument = space == std::string::npos
        ? std::string{}
        : Unquote(Trim(command.substr(space + 1)));

    if (operation == "w") {
        SaveFile(argument);
        return;
    }
    if (operation == "q") {
        if (gtk_text_buffer_get_modified(textBuffer_)) {
            SetStatus("No write since last change (use :q! to discard).");
            return;
        }
        g_application_quit(G_APPLICATION(app_));
        return;
    }
    if (operation == "q!") {
        gtk_text_buffer_set_modified(textBuffer_, FALSE);
        g_application_quit(G_APPLICATION(app_));
        return;
    }
    if (operation == "wq" || operation == "x") {
        if (SaveFile(argument)) g_application_quit(G_APPLICATION(app_));
        return;
    }
    if (operation == "e" || operation == "e!") {
        if (argument.empty()) {
            SetStatus("Usage: :e[!] <path>");
            return;
        }
        LoadFile(argument, operation == "e!");
        return;
    }
    if (operation == "new" || operation == "new!") {
        NewBuffer(operation == "new!");
        return;
    }
    if (operation == "help") {
        SetStatus(
            "NORMAL: hjkl/arrows w b 0 $ gg G i a I A o O x dd / n | "
            "Ex: :w :q :q! :wq :e :e! :new"
        );
        return;
    }

    SetStatus("Not an editor command: " + operation);
}

bool Application::LoadFile(const std::filesystem::path& path, bool force) {
    if (gtk_text_buffer_get_modified(textBuffer_) && !force) {
        SetStatus("No write since last change (use :e! <path> to discard).");
        return false;
    }

    std::string error;
    if (!editorBuffer_.Load(path, error)) {
        SetStatus(error);
        return false;
    }

    SyncGtkFromEditorBuffer();
    SetStatus("Opened " + editorBuffer_.Path().string());
    UpdateWindowTitle();
    return true;
}

bool Application::SaveFile(const std::filesystem::path& path) {
    if (path.empty() && !editorBuffer_.HasPath()) return SaveFileDialog();

    SyncEditorBufferFromGtk();

    std::string error;
    const bool saved = path.empty()
        ? editorBuffer_.Save(error)
        : editorBuffer_.SaveAs(path, error);

    if (!saved) {
        SetStatus(error);
        return false;
    }

    gtk_text_buffer_set_modified(textBuffer_, FALSE);
    SetStatus(
        "Written " + editorBuffer_.Path().string() + " (" +
        std::to_string(gtk_text_buffer_get_line_count(textBuffer_)) + " lines)"
    );
    UpdateWindowTitle();
    return true;
}

void Application::NewBuffer(bool force) {
    if (gtk_text_buffer_get_modified(textBuffer_) && !force) {
        SetStatus("No write since last change (use :new! to discard).");
        return;
    }

    editorBuffer_.NewEmpty();
    SyncGtkFromEditorBuffer();
    SetStatus("New buffer");
    UpdateWindowTitle();
}

void Application::OpenFileDialog() {
    if (!ConfirmDiscardOrSave()) return;

    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Open file",
        GTK_WINDOW(window_),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT,
        nullptr
    );

    if (editorBuffer_.HasPath()) {
        const auto parent = editorBuffer_.Path().parent_path();
        if (!parent.empty()) gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), parent.string().c_str());
    }

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            LoadFile(filename, true);
            g_free(filename);
        }
    }

    gtk_widget_destroy(dialog);
}

bool Application::SaveFileDialog() {
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Save file",
        GTK_WINDOW(window_),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Save", GTK_RESPONSE_ACCEPT,
        nullptr
    );
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    if (editorBuffer_.HasPath()) {
        gtk_file_chooser_set_filename(
            GTK_FILE_CHOOSER(dialog),
            editorBuffer_.Path().string().c_str()
        );
    }

    bool saved = false;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            saved = SaveFile(filename);
            g_free(filename);
        }
    }

    gtk_widget_destroy(dialog);
    return saved;
}

bool Application::ConfirmDiscardOrSave() {
    if (!textBuffer_ || !gtk_text_buffer_get_modified(textBuffer_)) return true;

    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(window_),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_NONE,
        "The current document has unsaved changes."
    );
    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dialog),
        "Save the document before continuing, discard the changes, or cancel."
    );
    gtk_dialog_add_buttons(
        GTK_DIALOG(dialog),
        "Cancel", GTK_RESPONSE_CANCEL,
        "Discard", GTK_RESPONSE_REJECT,
        "Save", GTK_RESPONSE_ACCEPT,
        nullptr
    );

    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_REJECT) {
        gtk_text_buffer_set_modified(textBuffer_, FALSE);
        return true;
    }
    if (response == GTK_RESPONSE_ACCEPT) return SaveFile();
    return false;
}

void Application::SyncGtkFromEditorBuffer() {
    if (!textBuffer_) return;
    suppressBufferChanged_ = true;
    const std::string text = editorBuffer_.Text();
    gtk_text_buffer_set_text(textBuffer_, text.c_str(), static_cast<gint>(text.size()));
    gtk_text_buffer_set_modified(textBuffer_, FALSE);

    GtkTextIter start;
    gtk_text_buffer_get_start_iter(textBuffer_, &start);
    gtk_text_buffer_place_cursor(textBuffer_, &start);
    suppressBufferChanged_ = false;
}

void Application::SyncEditorBufferFromGtk() {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(textBuffer_, &start, &end);
    char* text = gtk_text_buffer_get_text(textBuffer_, &start, &end, TRUE);
    editorBuffer_.SetText(text ? text : "", true);
    g_free(text);
}

GtkTextIter Application::CursorIter() const {
    GtkTextIter iter;
    GtkTextMark* insert = gtk_text_buffer_get_insert(textBuffer_);
    gtk_text_buffer_get_iter_at_mark(textBuffer_, &iter, insert);
    return iter;
}

void Application::PlaceCursor(GtkTextIter iter, bool scroll) {
    gtk_text_buffer_place_cursor(textBuffer_, &iter);
    if (scroll) {
        GtkTextMark* insert = gtk_text_buffer_get_insert(textBuffer_);
        gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(textView_), insert);
    }
    UpdateStatusBar();
}

void Application::MoveHorizontal(int direction) {
    GtkTextIter iter = CursorIter();
    if (direction < 0) {
        if (gtk_text_iter_get_line_offset(&iter) > 0) gtk_text_iter_backward_char(&iter);
    } else if (!gtk_text_iter_ends_line(&iter)) {
        gtk_text_iter_forward_char(&iter);
    }
    PlaceCursor(iter);
}

void Application::MoveVertical(int direction) {
    GtkTextIter current = CursorIter();
    const int currentLine = gtk_text_iter_get_line(&current);
    const int targetLine = currentLine + direction;
    const int lineCount = gtk_text_buffer_get_line_count(textBuffer_);
    if (targetLine < 0 || targetLine >= lineCount) return;

    const int desiredOffset = gtk_text_iter_get_line_offset(&current);
    GtkTextIter target;
    gtk_text_buffer_get_iter_at_line(textBuffer_, &target, targetLine);
    GtkTextIter lineEnd = target;
    gtk_text_iter_forward_to_line_end(&lineEnd);
    const int maximumOffset = gtk_text_iter_get_line_offset(&lineEnd);
    gtk_text_iter_set_line_offset(&target, std::min(desiredOffset, maximumOffset));
    PlaceCursor(target);
}

void Application::MoveWordForward() {
    GtkTextIter iter = CursorIter();
    if (!gtk_text_iter_is_end(&iter)) gtk_text_iter_forward_char(&iter);
    while (!gtk_text_iter_is_end(&iter) && !gtk_text_iter_starts_word(&iter)) {
        gtk_text_iter_forward_char(&iter);
    }
    PlaceCursor(iter);
}

void Application::MoveWordBackward() {
    GtkTextIter iter = CursorIter();
    if (!gtk_text_iter_is_start(&iter)) gtk_text_iter_backward_char(&iter);
    while (!gtk_text_iter_is_start(&iter) && !gtk_text_iter_starts_word(&iter)) {
        gtk_text_iter_backward_char(&iter);
    }
    PlaceCursor(iter);
}

void Application::MoveLineStart() {
    GtkTextIter iter = CursorIter();
    gtk_text_iter_set_line_offset(&iter, 0);
    PlaceCursor(iter);
}

void Application::MoveLineEnd(bool insertionPoint) {
    GtkTextIter iter = CursorIter();
    gtk_text_iter_forward_to_line_end(&iter);
    if (!insertionPoint && gtk_text_iter_get_line_offset(&iter) > 0) {
        gtk_text_iter_backward_char(&iter);
    }
    PlaceCursor(iter);
}

void Application::MoveDocumentStart() {
    GtkTextIter iter;
    gtk_text_buffer_get_start_iter(textBuffer_, &iter);
    PlaceCursor(iter);
}

void Application::MoveDocumentEnd() {
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(textBuffer_, &iter);
    PlaceCursor(iter);
}

void Application::DeleteCharacter() {
    GtkTextIter start = CursorIter();
    if (gtk_text_iter_ends_line(&start) || gtk_text_iter_is_end(&start)) return;
    GtkTextIter end = start;
    gtk_text_iter_forward_char(&end);
    gtk_text_buffer_delete(textBuffer_, &start, &end);
    PlaceCursor(start);
    SetStatus("Deleted character");
}

void Application::DeleteLine() {
    GtkTextIter cursor = CursorIter();
    const int line = gtk_text_iter_get_line(&cursor);
    const int lineCount = gtk_text_buffer_get_line_count(textBuffer_);

    GtkTextIter start;
    GtkTextIter end;

    if (lineCount <= 1) {
        gtk_text_buffer_get_bounds(textBuffer_, &start, &end);
    } else if (line + 1 < lineCount) {
        gtk_text_buffer_get_iter_at_line(textBuffer_, &start, line);
        gtk_text_buffer_get_iter_at_line(textBuffer_, &end, line + 1);
    } else {
        gtk_text_buffer_get_iter_at_line(textBuffer_, &start, line);
        gtk_text_iter_backward_char(&start);
        gtk_text_buffer_get_end_iter(textBuffer_, &end);
    }

    gtk_text_buffer_delete(textBuffer_, &start, &end);
    PlaceCursor(start);
    SetStatus("Deleted line");
}

void Application::OpenLineBelow() {
    GtkTextIter iter = CursorIter();
    gtk_text_iter_forward_to_line_end(&iter);
    gtk_text_buffer_insert(textBuffer_, &iter, "\n", 1);
    typingMetrics_.RecordLine();
    PlaceCursor(iter);
    EnterMode(Mode::Insert);
}

void Application::OpenLineAbove() {
    GtkTextIter iter = CursorIter();
    gtk_text_iter_set_line_offset(&iter, 0);
    GtkTextMark* mark = gtk_text_buffer_create_mark(textBuffer_, nullptr, &iter, TRUE);
    gtk_text_buffer_insert(textBuffer_, &iter, "\n", 1);
    typingMetrics_.RecordLine();

    GtkTextIter blankLine;
    gtk_text_buffer_get_iter_at_mark(textBuffer_, &blankLine, mark);
    gtk_text_buffer_delete_mark(textBuffer_, mark);
    PlaceCursor(blankLine);
    EnterMode(Mode::Insert);
}

bool Application::FindNext(const std::string& query, bool wrap) {
    if (query.empty()) return false;

    GtkTextIter start = CursorIter();
    if (!gtk_text_iter_is_end(&start)) gtk_text_iter_forward_char(&start);

    GtkTextIter matchStart;
    GtkTextIter matchEnd;
    gboolean found = gtk_text_iter_forward_search(
        &start,
        query.c_str(),
        GTK_TEXT_SEARCH_TEXT_ONLY,
        &matchStart,
        &matchEnd,
        nullptr
    );

    bool wrapped = false;
    if (!found && wrap) {
        gtk_text_buffer_get_start_iter(textBuffer_, &start);
        found = gtk_text_iter_forward_search(
            &start,
            query.c_str(),
            GTK_TEXT_SEARCH_TEXT_ONLY,
            &matchStart,
            &matchEnd,
            nullptr
        );
        wrapped = found != FALSE;
    }

    if (!found) return false;

    gtk_text_buffer_select_range(textBuffer_, &matchStart, &matchEnd);
    gtk_text_view_scroll_to_iter(
        GTK_TEXT_VIEW(textView_),
        &matchStart,
        0.15,
        FALSE,
        0.0,
        0.0
    );
    SetStatus("/" + query + (wrapped ? " (wrapped)" : ""));
    return true;
}

void Application::UpdateStatusBar() {
    if (!statusLabel_ || !metricsLabel_ || !textBuffer_) return;

    GtkTextIter iter = CursorIter();
    const int line = gtk_text_iter_get_line(&iter) + 1;
    const int column = gtk_text_iter_get_line_offset(&iter) + 1;
    const std::string path = editorBuffer_.HasPath()
        ? editorBuffer_.Path().string()
        : "[No Name]";

    std::ostringstream left;
    left << ModeName() << "  " << path;
    if (gtk_text_buffer_get_modified(textBuffer_)) left << " [+]";
    left << "   " << line << ':' << column;
    if (!status_.empty()) left << "   |   " << status_;
    gtk_label_set_text(GTK_LABEL(statusLabel_), left.str().c_str());

    const auto metrics = typingMetrics_.GetSnapshot();
    std::ostringstream right;
    right << "LPM " << metrics.linesLastMinute
          << "   LPH " << metrics.linesLastHour
          << "   CPM30 " << static_cast<unsigned long long>(
                 metrics.charactersPerMinute30Seconds + 0.5
             );
    gtk_label_set_text(GTK_LABEL(metricsLabel_), right.str().c_str());
}

void Application::UpdateWindowTitle() {
    if (!window_) return;
    const std::string name = editorBuffer_.HasPath()
        ? editorBuffer_.Path().filename().string()
        : "Untitled";
    const std::string modified = textBuffer_ && gtk_text_buffer_get_modified(textBuffer_)
        ? " *"
        : "";
    const std::string title = name + modified + " - SentinelEditor";
    gtk_window_set_title(GTK_WINDOW(window_), title.c_str());
}

void Application::SetStatus(std::string status) {
    status_ = std::move(status);
    UpdateStatusBar();
}

std::string Application::ModeName() const {
    switch (mode_) {
        case Mode::Normal: return "NORMAL";
        case Mode::Insert: return "INSERT";
        case Mode::Command: return "COMMAND";
        case Mode::Search: return "SEARCH";
    }
    return "NORMAL";
}

std::string Application::Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string Application::Unquote(std::string value) {
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}
