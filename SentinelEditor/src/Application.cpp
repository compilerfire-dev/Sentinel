#include "Application.hpp"
#include "FuzzySearch.hpp"

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

bool HasCtrlShift(GdkEventKey* event, guint key) {
    return event && event->keyval == key &&
        (event->state & GDK_CONTROL_MASK) != 0 &&
        (event->state & GDK_SHIFT_MASK) != 0;
}
}

int Application::Run(int argc, char** argv) {
    if (argc > 1 && argv[1] && *argv[1]) initialPath_ = argv[1];

    app_ = gtk_application_new("dev.compilerfire.sentinel.editor", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app_, "activate", G_CALLBACK(OnActivate), this);

    char* gtkArgv[] = {
        (argc > 0 && argv && argv[0]) ? argv[0] : const_cast<char*>("SentinelEditor"),
        nullptr
    };
    const int result = g_application_run(G_APPLICATION(app_), 1, gtkArgv);

    if (refreshTimerId_ != 0) g_source_remove(refreshTimerId_);
    if (paletteWindow_) gtk_widget_destroy(paletteWindow_);
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

void Application::OnMenuCommand(GtkMenuItem* item, gpointer userData) {
    auto* self = static_cast<Application*>(userData);
    const char* id = static_cast<const char*>(g_object_get_data(G_OBJECT(item), "sentinel-command"));
    if (id) self->ExecuteAction(id);
}

void Application::OnPaletteChanged(GtkEditable*, gpointer userData) {
    static_cast<Application*>(userData)->RefreshCommandPalette();
}

void Application::OnPaletteRowActivated(GtkListBox*, GtkListBoxRow* row, gpointer userData) {
    auto* self = static_cast<Application*>(userData);
    if (!row) return;
    const int rowIndex = gtk_list_box_row_get_index(row);
    if (rowIndex < 0 || static_cast<std::size_t>(rowIndex) >= self->paletteResultIndices_.size()) return;
    const auto commandIndex = self->paletteResultIndices_[static_cast<std::size_t>(rowIndex)];
    if (commandIndex >= self->commands_.size()) return;
    const std::string id = self->commands_[commandIndex].id;
    gtk_widget_hide(self->paletteWindow_);
    self->ExecuteAction(id);
}

gboolean Application::OnPaletteKeyPress(GtkWidget*, GdkEventKey* event, gpointer userData) {
    auto* self = static_cast<Application*>(userData);
    if (!event) return FALSE;
    if (event->keyval == GDK_KEY_Escape) {
        gtk_widget_hide(self->paletteWindow_);
        gtk_widget_grab_focus(self->textView_);
        return TRUE;
    }
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        self->ExecuteSelectedPaletteCommand();
        return TRUE;
    }
    if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_Up) {
        GtkListBoxRow* selected = gtk_list_box_get_selected_row(GTK_LIST_BOX(self->paletteList_));
        int index = selected ? gtk_list_box_row_get_index(selected) : 0;
        index += event->keyval == GDK_KEY_Down ? 1 : -1;
        const int count = static_cast<int>(self->paletteResultIndices_.size());
        if (count > 0) {
            index = std::clamp(index, 0, count - 1);
            GtkListBoxRow* row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(self->paletteList_), index);
            if (row) gtk_list_box_select_row(GTK_LIST_BOX(self->paletteList_), row);
        }
        return TRUE;
    }
    return FALSE;
}

void Application::BuildWindow(GtkApplication* app) {
    commands_ = {
        {"file.new", "File: New", "Create a new empty document"},
        {"file.open", "File: Open", "Open a file from disk"},
        {"file.save", "File: Save", "Save the current document"},
        {"file.saveAs", "File: Save As", "Save to a different path"},
        {"file.quit", "File: Quit", "Close SentinelEditor"},
        {"editor.find", "Editor: Find", "Search in the current document"},
        {"editor.settings", "Settings: Open Settings", "Configure SentinelEditor"},
        {"editor.toggleLineNumbers", "View: Toggle Line Numbers", "Show or hide the line-number gutter"},
        {"editor.toggleMetrics", "View: Toggle Typing Metrics", "Show or hide programming-speed metrics"},
        {"editor.toggleVim", "Editor: Toggle Vim Mode", "Enable or disable modal Vim-style controls"},
        {"help.keyboard", "Help: Keyboard Shortcuts", "Show Vim and editor shortcuts"},
        {"help.about", "Help: About SentinelEditor", "Show application information"}
    };

    window_ = gtk_application_window_new(app);
    gtk_window_set_default_size(GTK_WINDOW(window_), 1400, 900);
    g_signal_connect(window_, "delete-event", G_CALLBACK(OnWindowDelete), this);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window_), root);
    gtk_box_pack_start(GTK_BOX(root), BuildMenuBar(), FALSE, FALSE, 0);

    GtkWidget* editorRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(editorRow, TRUE);
    gtk_widget_set_vexpand(editorRow, TRUE);
    gtk_box_pack_start(GTK_BOX(root), editorRow, TRUE, TRUE, 0);

    GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);

    textView_ = gtk_text_view_new();
    textBuffer_ = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textView_));
    gtk_widget_set_name(textView_, "sentinel-editor-text");
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(textView_), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textView_), GTK_WRAP_NONE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textView_), 12);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(textView_), 12);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(textView_), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(textView_), 10);
    gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(textView_), TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled), textView_);

    lineNumberGutter_.Attach(GTK_TEXT_VIEW(textView_), GTK_SCROLLED_WINDOW(scrolled));
    gtk_box_pack_start(GTK_BOX(editorRow), lineNumberGutter_.Widget(), FALSE, FALSE, 0);
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

    SyncGtkFromEditorBuffer();
    if (!initialPath_.empty()) LoadFile(initialPath_, true);
    else SetStatus("Ready. Ctrl+Shift+P opens the Command Palette.");

    refreshTimerId_ = g_timeout_add(RefreshIntervalMilliseconds, OnRefreshTimer, this);
    gtk_widget_show_all(window_);
    gtk_widget_hide(commandBox_);
    ApplySettings();
    EnterMode(Mode::Normal);
    UpdateWindowTitle();
    UpdateStatusBar();
}

GtkWidget* Application::BuildMenuBar() {
    GtkWidget* bar = gtk_menu_bar_new();

    GtkWidget* fileItem = gtk_menu_item_new_with_label("File");
    GtkWidget* fileMenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(fileItem), fileMenu);
    AddMenuItem(fileMenu, "New", "file.new");
    AddMenuItem(fileMenu, "Open...", "file.open");
    AddMenuItem(fileMenu, "Save", "file.save");
    AddMenuItem(fileMenu, "Save As...", "file.saveAs");
    gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), gtk_separator_menu_item_new());
    AddMenuItem(fileMenu, "Quit", "file.quit");

    GtkWidget* settingsItem = gtk_menu_item_new_with_label("Settings");
    GtkWidget* settingsMenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(settingsItem), settingsMenu);
    AddMenuItem(settingsMenu, "Command Palette...    Ctrl+Shift+P", "commandPalette");
    AddMenuItem(settingsMenu, "Settings...", "editor.settings");
    gtk_menu_shell_append(GTK_MENU_SHELL(settingsMenu), gtk_separator_menu_item_new());
    AddMenuItem(settingsMenu, "Toggle Line Numbers", "editor.toggleLineNumbers");
    AddMenuItem(settingsMenu, "Toggle Typing Metrics", "editor.toggleMetrics");
    AddMenuItem(settingsMenu, "Toggle Vim Mode", "editor.toggleVim");

    GtkWidget* helpItem = gtk_menu_item_new_with_label("Help");
    GtkWidget* helpMenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(helpItem), helpMenu);
    AddMenuItem(helpMenu, "Keyboard Shortcuts", "help.keyboard");
    AddMenuItem(helpMenu, "About SentinelEditor", "help.about");

    gtk_menu_shell_append(GTK_MENU_SHELL(bar), fileItem);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), settingsItem);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), helpItem);
    return bar;
}

GtkWidget* Application::AddMenuItem(GtkWidget* menu, const char* label, const char* commandId, guint, GdkModifierType) {
    GtkWidget* item = gtk_menu_item_new_with_label(label);
    g_object_set_data_full(G_OBJECT(item), "sentinel-command", g_strdup(commandId), g_free);
    g_signal_connect(item, "activate", G_CALLBACK(OnMenuCommand), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    return item;
}

gboolean Application::HandleTextKeyPress(GdkEventKey* event) {
    if (!event) return FALSE;
    if (HasCtrlShift(event, GDK_KEY_P) || HasCtrlShift(event, GDK_KEY_p)) {
        OpenCommandPalette();
        return TRUE;
    }
    if ((event->state & GDK_CONTROL_MASK) != 0 && (event->keyval == GDK_KEY_s || event->keyval == GDK_KEY_S)) {
        SaveFile();
        return TRUE;
    }
    if (!settings_.vimMode) return FALSE;
    if (mode_ == Mode::Insert) return HandleInsertKey(event);
    if (mode_ == Mode::Normal) return HandleNormalKey(event);
    return TRUE;
}

gboolean Application::HandleInsertKey(GdkEventKey* event) {
    if (event->keyval == GDK_KEY_Escape) { EnterMode(Mode::Normal); return TRUE; }
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) typingMetrics_.RecordLine();
    else if (event->keyval == GDK_KEY_Tab) typingMetrics_.RecordCharacter();
    else if (!HasEditingModifier(static_cast<GdkModifierType>(event->state))) {
        const gunichar c = gdk_keyval_to_unicode(event->keyval);
        if (c >= 0x20U && c != 0x7FU) typingMetrics_.RecordCharacter();
    }
    return FALSE;
}

gboolean Application::HandleNormalKey(GdkEventKey* event) {
    const guint key = event->keyval;
    if (pendingDelete_) {
        pendingDelete_ = false;
        if (key == GDK_KEY_d) DeleteLine();
        return TRUE;
    }
    if (pendingGoto_) {
        pendingGoto_ = false;
        if (key == GDK_KEY_g) MoveDocumentStart();
        return TRUE;
    }

    switch (key) {
        case GDK_KEY_Left: case GDK_KEY_h: MoveHorizontal(-1); return TRUE;
        case GDK_KEY_Right: case GDK_KEY_l: MoveHorizontal(1); return TRUE;
        case GDK_KEY_Up: case GDK_KEY_k: MoveVertical(-1); return TRUE;
        case GDK_KEY_Down: case GDK_KEY_j: MoveVertical(1); return TRUE;
        case GDK_KEY_0: case GDK_KEY_Home: MoveLineStart(); return TRUE;
        case GDK_KEY_dollar: case GDK_KEY_End: MoveLineEnd(false); return TRUE;
        case GDK_KEY_w: MoveWordForward(); return TRUE;
        case GDK_KEY_b: MoveWordBackward(); return TRUE;
        case GDK_KEY_g: pendingGoto_ = true; SetStatus("g"); return TRUE;
        case GDK_KEY_G: MoveDocumentEnd(); return TRUE;
        case GDK_KEY_i: EnterMode(Mode::Insert); return TRUE;
        case GDK_KEY_a: { auto iter = CursorIter(); if (!gtk_text_iter_ends_line(&iter)) gtk_text_iter_forward_char(&iter); PlaceCursor(iter, false); EnterMode(Mode::Insert); return TRUE; }
        case GDK_KEY_I: MoveLineStart(); EnterMode(Mode::Insert); return TRUE;
        case GDK_KEY_A: MoveLineEnd(true); EnterMode(Mode::Insert); return TRUE;
        case GDK_KEY_o: OpenLineBelow(); return TRUE;
        case GDK_KEY_O: OpenLineAbove(); return TRUE;
        case GDK_KEY_x: DeleteCharacter(); return TRUE;
        case GDK_KEY_d: pendingDelete_ = true; SetStatus("d"); return TRUE;
        case GDK_KEY_colon: EnterMode(Mode::Command); return TRUE;
        case GDK_KEY_slash: EnterMode(Mode::Search); return TRUE;
        case GDK_KEY_n: if (!lastSearch_.empty()) FindNext(lastSearch_, true); return TRUE;
        default: return TRUE;
    }
}

gboolean Application::HandleCommandEntryKey(GdkEventKey* event) {
    if (!event) return FALSE;
    if (event->keyval == GDK_KEY_Escape) { EnterMode(Mode::Normal); return TRUE; }
    if (event->keyval != GDK_KEY_Return && event->keyval != GDK_KEY_KP_Enter) return FALSE;
    const std::string value = gtk_entry_get_text(GTK_ENTRY(commandEntry_));
    const Mode previous = mode_;
    EnterMode(Mode::Normal);
    if (previous == Mode::Command) ExecuteCommand(value);
    else if (previous == Mode::Search && !value.empty()) { lastSearch_ = value; FindNext(value, true); }
    return TRUE;
}

void Application::EnterMode(Mode mode) {
    mode_ = mode;
    pendingDelete_ = pendingGoto_ = false;
    if (!textView_) return;

    if (!settings_.vimMode) {
        gtk_text_view_set_editable(GTK_TEXT_VIEW(textView_), TRUE);
        gtk_widget_hide(commandBox_);
        gtk_widget_grab_focus(textView_);
        mode_ = Mode::Insert;
    } else if (mode_ == Mode::Insert) {
        gtk_text_view_set_editable(GTK_TEXT_VIEW(textView_), TRUE);
        gtk_widget_hide(commandBox_);
        gtk_widget_grab_focus(textView_);
    } else if (mode_ == Mode::Normal) {
        gtk_text_view_set_editable(GTK_TEXT_VIEW(textView_), FALSE);
        gtk_widget_hide(commandBox_);
        gtk_widget_grab_focus(textView_);
    } else {
        gtk_text_view_set_editable(GTK_TEXT_VIEW(textView_), FALSE);
        gtk_label_set_text(GTK_LABEL(commandPromptLabel_), mode_ == Mode::Command ? ":" : "/");
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
    const std::string op = space == std::string::npos ? command : command.substr(0, space);
    const std::string argument = space == std::string::npos ? std::string{} : Unquote(Trim(command.substr(space + 1)));
    if (op == "w") SaveFile(argument);
    else if (op == "q") { if (gtk_text_buffer_get_modified(textBuffer_)) SetStatus("Unsaved changes; use :q! to discard."); else g_application_quit(G_APPLICATION(app_)); }
    else if (op == "q!") { gtk_text_buffer_set_modified(textBuffer_, FALSE); g_application_quit(G_APPLICATION(app_)); }
    else if (op == "wq" || op == "x") { if (SaveFile(argument)) g_application_quit(G_APPLICATION(app_)); }
    else if (op == "e" || op == "e!") { if (!argument.empty()) LoadFile(argument, op == "e!"); }
    else if (op == "new" || op == "new!") NewBuffer(op == "new!");
    else if (op == "settings") ShowSettingsDialog();
    else if (op == "palette") OpenCommandPalette();
    else if (op == "help") ShowHelpDialog();
    else SetStatus("Not an editor command: " + op);
}

void Application::ExecuteAction(const std::string& id) {
    if (id == "commandPalette") OpenCommandPalette();
    else if (id == "file.new") NewBuffer(false);
    else if (id == "file.open") OpenFileDialog();
    else if (id == "file.save") SaveFile();
    else if (id == "file.saveAs") SaveFileDialog();
    else if (id == "file.quit") { if (ConfirmDiscardOrSave()) g_application_quit(G_APPLICATION(app_)); }
    else if (id == "editor.find") EnterMode(Mode::Search);
    else if (id == "editor.settings") ShowSettingsDialog();
    else if (id == "editor.toggleLineNumbers") { settings_.showLineNumbers = !settings_.showLineNumbers; ApplySettings(); }
    else if (id == "editor.toggleMetrics") { settings_.showTypingMetrics = !settings_.showTypingMetrics; ApplySettings(); }
    else if (id == "editor.toggleVim") { settings_.vimMode = !settings_.vimMode; ApplySettings(); }
    else if (id == "help.keyboard") ShowHelpDialog();
    else if (id == "help.about") {
        gtk_show_about_dialog(GTK_WINDOW(window_), "program-name", "SentinelEditor", "version", "0.1", "comments", "GTK+ 3 Vim-inspired editor in the Sentinel suite.", nullptr);
    }
}

void Application::OpenCommandPalette() {
    if (!paletteWindow_) {
        paletteWindow_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(paletteWindow_), "Command Palette");
        gtk_window_set_transient_for(GTK_WINDOW(paletteWindow_), GTK_WINDOW(window_));
        gtk_window_set_modal(GTK_WINDOW(paletteWindow_), TRUE);
        gtk_window_set_default_size(GTK_WINDOW(paletteWindow_), 700, 420);
        gtk_window_set_position(GTK_WINDOW(paletteWindow_), GTK_WIN_POS_CENTER_ON_PARENT);

        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width(GTK_CONTAINER(box), 10);
        gtk_container_add(GTK_CONTAINER(paletteWindow_), box);
        paletteEntry_ = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(paletteEntry_), "Type a command, e.g. save, settings, line numbers...");
        paletteList_ = gtk_list_box_new();
        gtk_list_box_set_selection_mode(GTK_LIST_BOX(paletteList_), GTK_SELECTION_SINGLE);
        GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_container_add(GTK_CONTAINER(scroll), paletteList_);
        gtk_widget_set_vexpand(scroll, TRUE);
        gtk_box_pack_start(GTK_BOX(box), paletteEntry_, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);
        g_signal_connect(paletteEntry_, "changed", G_CALLBACK(OnPaletteChanged), this);
        g_signal_connect(paletteEntry_, "key-press-event", G_CALLBACK(OnPaletteKeyPress), this);
        g_signal_connect(paletteList_, "row-activated", G_CALLBACK(OnPaletteRowActivated), this);
        g_signal_connect(paletteList_, "key-press-event", G_CALLBACK(OnPaletteKeyPress), this);
        g_signal_connect_swapped(paletteWindow_, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), paletteWindow_);
        gtk_widget_show_all(paletteWindow_);
    } else gtk_widget_show_all(paletteWindow_);

    gtk_entry_set_text(GTK_ENTRY(paletteEntry_), "");
    RefreshCommandPalette();
    gtk_widget_grab_focus(paletteEntry_);
}

void Application::RefreshCommandPalette() {
    if (!paletteList_ || !paletteEntry_) return;
    GList* children = gtk_container_get_children(GTK_CONTAINER(paletteList_));
    for (GList* it = children; it; it = it->next) gtk_widget_destroy(GTK_WIDGET(it->data));
    g_list_free(children);

    const std::string query = gtk_entry_get_text(GTK_ENTRY(paletteEntry_));
    std::vector<std::string> labels;
    labels.reserve(commands_.size());
    for (const auto& command : commands_) labels.push_back(command.label + " " + command.detail);
    const auto ranked = FuzzySearch::Rank(query, labels);

    paletteResultIndices_.clear();
    const std::size_t limit = std::min<std::size_t>(12, ranked.size());
    for (std::size_t i = 0; i < limit; ++i) {
        const auto commandIndex = ranked[i].index;
        paletteResultIndices_.push_back(commandIndex);
        GtkWidget* rowBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget* title = gtk_label_new(commands_[commandIndex].label.c_str());
        GtkWidget* detail = gtk_label_new(commands_[commandIndex].detail.c_str());
        gtk_label_set_xalign(GTK_LABEL(title), 0.0F);
        gtk_label_set_xalign(GTK_LABEL(detail), 0.0F);
        gtk_style_context_add_class(gtk_widget_get_style_context(detail), "dim-label");
        gtk_container_set_border_width(GTK_CONTAINER(rowBox), 6);
        gtk_box_pack_start(GTK_BOX(rowBox), title, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(rowBox), detail, FALSE, FALSE, 0);
        gtk_list_box_insert(GTK_LIST_BOX(paletteList_), rowBox, -1);
    }
    gtk_widget_show_all(paletteList_);
    if (!paletteResultIndices_.empty()) {
        GtkListBoxRow* first = gtk_list_box_get_row_at_index(GTK_LIST_BOX(paletteList_), 0);
        gtk_list_box_select_row(GTK_LIST_BOX(paletteList_), first);
    }
}

void Application::ExecuteSelectedPaletteCommand() {
    GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(paletteList_));
    if (!row) return;
    OnPaletteRowActivated(GTK_LIST_BOX(paletteList_), row, this);
}

void Application::ShowSettingsDialog() {
    const auto dialogFlags = static_cast<GtkDialogFlags>(
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT
    );
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "SentinelEditor Settings",
        GTK_WINDOW(window_),
        dialogFlags,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Apply", GTK_RESPONSE_APPLY,
        "OK", GTK_RESPONSE_OK,
        nullptr
    );
    gtk_window_set_default_size(GTK_WINDOW(dialog), 480, 320);
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 18);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 18);
    gtk_box_pack_start(GTK_BOX(content), grid, TRUE, TRUE, 0);

    GtkWidget* fontLabel = gtk_label_new("Editor font size:");
    GtkWidget* fontSpin = gtk_spin_button_new_with_range(8, 32, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(fontSpin), settings_.fontSize);
    GtkWidget* lines = gtk_check_button_new_with_label("Show line-number gutter");
    GtkWidget* metrics = gtk_check_button_new_with_label("Show typing metrics (LPM / LPH / CPM30)");
    GtkWidget* vim = gtk_check_button_new_with_label("Enable Vim-style modal editing");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lines), settings_.showLineNumbers);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(metrics), settings_.showTypingMetrics);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vim), settings_.vimMode);

    gtk_grid_attach(GTK_GRID(grid), fontLabel, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), fontSpin, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), lines, 0, 1, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), metrics, 0, 2, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), vim, 0, 3, 2, 1);
    gtk_widget_show_all(dialog);

    while (true) {
        const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        if (response != GTK_RESPONSE_APPLY && response != GTK_RESPONSE_OK) break;
        settings_.fontSize = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(fontSpin));
        settings_.showLineNumbers = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lines));
        settings_.showTypingMetrics = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(metrics));
        settings_.vimMode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vim));
        ApplySettings();
        if (response == GTK_RESPONSE_OK) break;
    }
    gtk_widget_destroy(dialog);
}

void Application::ShowHelpDialog() {
    GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(window_), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
        "SentinelEditor shortcuts\n\nCtrl+Shift+P  Command Palette\nCtrl+S        Save\n\nVim mode:\nh j k l  Move\nw / b    Word movement\ni / a    Insert\no / O    Open line\nx        Delete character\ndd       Delete line\n/        Find\n:        Command entry\n\nEx: :w :q :q! :wq :e :new :settings :palette");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void Application::ApplySettings() {
    if (!textView_) return;
    std::ostringstream css;
    css << "#sentinel-editor-text { font-family: monospace; font-size: " << settings_.fontSize << "pt; }";
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css.str().c_str(), -1, nullptr);
    gtk_style_context_add_provider(gtk_widget_get_style_context(textView_), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    gtk_widget_set_visible(lineNumberGutter_.Widget(), settings_.showLineNumbers);
    gtk_widget_set_visible(metricsLabel_, settings_.showTypingMetrics);
    EnterMode(settings_.vimMode ? Mode::Normal : Mode::Insert);
    SetStatus(settings_.vimMode ? "Vim mode enabled." : "Vim mode disabled; direct GTK editing enabled.");
}

bool Application::LoadFile(const std::filesystem::path& path, bool force) {
    if (gtk_text_buffer_get_modified(textBuffer_) && !force) { SetStatus("Unsaved changes; use :e! to discard."); return false; }
    std::string error;
    if (!editorBuffer_.Load(path, error)) { SetStatus(error); return false; }
    SyncGtkFromEditorBuffer();
    SetStatus("Opened " + editorBuffer_.Path().string());
    UpdateWindowTitle();
    return true;
}

bool Application::SaveFile(const std::filesystem::path& path) {
    if (path.empty() && !editorBuffer_.HasPath()) return SaveFileDialog();
    SyncEditorBufferFromGtk();
    std::string error;
    const bool saved = path.empty() ? editorBuffer_.Save(error) : editorBuffer_.SaveAs(path, error);
    if (!saved) { SetStatus(error); return false; }
    gtk_text_buffer_set_modified(textBuffer_, FALSE);
    SetStatus("Written " + editorBuffer_.Path().string());
    UpdateWindowTitle();
    return true;
}

void Application::NewBuffer(bool force) {
    if (gtk_text_buffer_get_modified(textBuffer_) && !force) { SetStatus("Unsaved changes; use :new! to discard."); return; }
    editorBuffer_.NewEmpty(); SyncGtkFromEditorBuffer(); SetStatus("New buffer"); UpdateWindowTitle();
}

void Application::OpenFileDialog() {
    if (!ConfirmDiscardOrSave()) return;
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Open file", GTK_WINDOW(window_), GTK_FILE_CHOOSER_ACTION_OPEN, "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, nullptr);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) { LoadFile(filename, true); g_free(filename); }
    }
    gtk_widget_destroy(dialog);
}

bool Application::SaveFileDialog() {
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Save file", GTK_WINDOW(window_), GTK_FILE_CHOOSER_ACTION_SAVE, "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT, nullptr);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    bool saved = false;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) { saved = SaveFile(filename); g_free(filename); }
    }
    gtk_widget_destroy(dialog);
    return saved;
}

bool Application::ConfirmDiscardOrSave() {
    if (!textBuffer_ || !gtk_text_buffer_get_modified(textBuffer_)) return true;
    GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(window_), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE, "The current document has unsaved changes.");
    gtk_dialog_add_buttons(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL, "Discard", GTK_RESPONSE_REJECT, "Save", GTK_RESPONSE_ACCEPT, nullptr);
    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (response == GTK_RESPONSE_REJECT) { gtk_text_buffer_set_modified(textBuffer_, FALSE); return true; }
    if (response == GTK_RESPONSE_ACCEPT) return SaveFile();
    return false;
}

void Application::SyncGtkFromEditorBuffer() {
    suppressBufferChanged_ = true;
    const auto text = editorBuffer_.Text();
    gtk_text_buffer_set_text(textBuffer_, text.c_str(), static_cast<gint>(text.size()));
    gtk_text_buffer_set_modified(textBuffer_, FALSE);
    GtkTextIter start; gtk_text_buffer_get_start_iter(textBuffer_, &start); gtk_text_buffer_place_cursor(textBuffer_, &start);
    suppressBufferChanged_ = false;
}

void Application::SyncEditorBufferFromGtk() {
    GtkTextIter start, end; gtk_text_buffer_get_bounds(textBuffer_, &start, &end);
    char* text = gtk_text_buffer_get_text(textBuffer_, &start, &end, TRUE);
    editorBuffer_.SetText(text ? text : "", true); g_free(text);
}

GtkTextIter Application::CursorIter() const { GtkTextIter iter; gtk_text_buffer_get_iter_at_mark(textBuffer_, &iter, gtk_text_buffer_get_insert(textBuffer_)); return iter; }
void Application::PlaceCursor(GtkTextIter iter, bool scroll) { gtk_text_buffer_place_cursor(textBuffer_, &iter); if (scroll) gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(textView_), gtk_text_buffer_get_insert(textBuffer_)); UpdateStatusBar(); }
void Application::MoveHorizontal(int d) { auto i=CursorIter(); if(d<0){if(gtk_text_iter_get_line_offset(&i)>0)gtk_text_iter_backward_char(&i);}else if(!gtk_text_iter_ends_line(&i))gtk_text_iter_forward_char(&i); PlaceCursor(i); }
void Application::MoveVertical(int d) { auto c=CursorIter(); int line=gtk_text_iter_get_line(&c)+d; if(line<0||line>=gtk_text_buffer_get_line_count(textBuffer_))return; int off=gtk_text_iter_get_line_offset(&c); GtkTextIter t,e; gtk_text_buffer_get_iter_at_line(textBuffer_,&t,line); e=t; gtk_text_iter_forward_to_line_end(&e); gtk_text_iter_set_line_offset(&t,std::min(off,gtk_text_iter_get_line_offset(&e))); PlaceCursor(t); }
void Application::MoveWordForward(){auto i=CursorIter(); if(!gtk_text_iter_is_end(&i))gtk_text_iter_forward_char(&i); while(!gtk_text_iter_is_end(&i)&&!gtk_text_iter_starts_word(&i))gtk_text_iter_forward_char(&i); PlaceCursor(i);}
void Application::MoveWordBackward(){auto i=CursorIter(); if(!gtk_text_iter_is_start(&i))gtk_text_iter_backward_char(&i); while(!gtk_text_iter_is_start(&i)&&!gtk_text_iter_starts_word(&i))gtk_text_iter_backward_char(&i); PlaceCursor(i);}
void Application::MoveLineStart(){auto i=CursorIter();gtk_text_iter_set_line_offset(&i,0);PlaceCursor(i);}
void Application::MoveLineEnd(bool insertion){auto i=CursorIter();gtk_text_iter_forward_to_line_end(&i);if(!insertion&&gtk_text_iter_get_line_offset(&i)>0)gtk_text_iter_backward_char(&i);PlaceCursor(i);}
void Application::MoveDocumentStart(){GtkTextIter i;gtk_text_buffer_get_start_iter(textBuffer_,&i);PlaceCursor(i);}
void Application::MoveDocumentEnd(){GtkTextIter i;gtk_text_buffer_get_end_iter(textBuffer_,&i);PlaceCursor(i);}
void Application::DeleteCharacter(){auto s=CursorIter();if(gtk_text_iter_ends_line(&s)||gtk_text_iter_is_end(&s))return;auto e=s;gtk_text_iter_forward_char(&e);gtk_text_buffer_delete(textBuffer_,&s,&e);PlaceCursor(s);}
void Application::DeleteLine(){auto c=CursorIter();int line=gtk_text_iter_get_line(&c), count=gtk_text_buffer_get_line_count(textBuffer_);GtkTextIter s,e;if(count<=1)gtk_text_buffer_get_bounds(textBuffer_,&s,&e);else if(line+1<count){gtk_text_buffer_get_iter_at_line(textBuffer_,&s,line);gtk_text_buffer_get_iter_at_line(textBuffer_,&e,line+1);}else{gtk_text_buffer_get_iter_at_line(textBuffer_,&s,line);gtk_text_iter_backward_char(&s);gtk_text_buffer_get_end_iter(textBuffer_,&e);}gtk_text_buffer_delete(textBuffer_,&s,&e);PlaceCursor(s);}
void Application::OpenLineBelow(){auto i=CursorIter();gtk_text_iter_forward_to_line_end(&i);gtk_text_buffer_insert(textBuffer_,&i,"\n",1);typingMetrics_.RecordLine();PlaceCursor(i);EnterMode(Mode::Insert);}
void Application::OpenLineAbove(){auto i=CursorIter();gtk_text_iter_set_line_offset(&i,0);GtkTextMark* m=gtk_text_buffer_create_mark(textBuffer_,nullptr,&i,TRUE);gtk_text_buffer_insert(textBuffer_,&i,"\n",1);typingMetrics_.RecordLine();GtkTextIter b;gtk_text_buffer_get_iter_at_mark(textBuffer_,&b,m);gtk_text_buffer_delete_mark(textBuffer_,m);PlaceCursor(b);EnterMode(Mode::Insert);}

bool Application::FindNext(const std::string& query, bool wrap){if(query.empty())return false;auto s=CursorIter();if(!gtk_text_iter_is_end(&s))gtk_text_iter_forward_char(&s);GtkTextIter ms,me;gboolean found=gtk_text_iter_forward_search(&s,query.c_str(),GTK_TEXT_SEARCH_TEXT_ONLY,&ms,&me,nullptr);if(!found&&wrap){gtk_text_buffer_get_start_iter(textBuffer_,&s);found=gtk_text_iter_forward_search(&s,query.c_str(),GTK_TEXT_SEARCH_TEXT_ONLY,&ms,&me,nullptr);}if(!found){SetStatus("Pattern not found: "+query);return false;}gtk_text_buffer_select_range(textBuffer_,&ms,&me);gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(textView_),&ms,0.15,FALSE,0,0);return true;}

void Application::UpdateStatusBar(){if(!statusLabel_||!metricsLabel_||!textBuffer_)return;auto i=CursorIter();std::ostringstream l;l<<ModeName()<<"  "<<(editorBuffer_.HasPath()?editorBuffer_.Path().string():"[No Name]");if(gtk_text_buffer_get_modified(textBuffer_))l<<" [+]";l<<"   "<<gtk_text_iter_get_line(&i)+1<<':'<<gtk_text_iter_get_line_offset(&i)+1;if(!status_.empty())l<<"   |   "<<status_;gtk_label_set_text(GTK_LABEL(statusLabel_),l.str().c_str());auto m=typingMetrics_.GetSnapshot();std::ostringstream r;r<<"LPM "<<m.linesLastMinute<<"   LPH "<<m.linesLastHour<<"   CPM30 "<<static_cast<unsigned long long>(m.charactersPerMinute30Seconds+0.5);gtk_label_set_text(GTK_LABEL(metricsLabel_),r.str().c_str());}
void Application::UpdateWindowTitle(){if(!window_)return;std::string n=editorBuffer_.HasPath()?editorBuffer_.Path().filename().string():"Untitled";if(textBuffer_&&gtk_text_buffer_get_modified(textBuffer_))n+=" *";n+=" - SentinelEditor";gtk_window_set_title(GTK_WINDOW(window_),n.c_str());}
void Application::SetStatus(std::string status){status_=std::move(status);UpdateStatusBar();}
std::string Application::ModeName() const {if(!settings_.vimMode)return "EDIT";switch(mode_){case Mode::Normal:return "NORMAL";case Mode::Insert:return "INSERT";case Mode::Command:return "COMMAND";case Mode::Search:return "SEARCH";}return "NORMAL";}
std::string Application::Trim(std::string v){auto f=v.find_first_not_of(" \t\r\n");if(f==std::string::npos)return{};auto l=v.find_last_not_of(" \t\r\n");return v.substr(f,l-f+1);}
std::string Application::Unquote(std::string v){if(v.size()>=2&&((v.front()=='"'&&v.back()=='"')||(v.front()=='\''&&v.back()=='\'')))v=v.substr(1,v.size()-2);return v;}
