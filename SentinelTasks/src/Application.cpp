#include "Application.hpp"

#include <algorithm>
#include <array>
#include <clocale>
#include <cctype>
#include <ncurses.h>
#include <sstream>
#include <string_view>

namespace {

struct CommandDefinition {
    std::string_view name;
    std::string_view description;
};

constexpr std::array<CommandDefinition, 11> Commands{{
    {"addFolder", "add a folder node"},
    {"addTask", "add an individual task"},
    {"remove", "remove a node and its subtree"},
    {"setDescription", "set a node description"},
    {"setEmoji", "change a node emoji"},
    {"manualSelect", "select nodes with arrows/mouse"},
    {"select", "select a node by ID"},
    {"emojis", "show allowed emoji markers"},
    {"commands", "show all commands"},
    {"list", "return to the tree view"},
    {"quit", "exit SentinelTasks"},
}};

constexpr std::size_t MaxSuggestions = 6;
constexpr std::size_t MaxDropdownRows = 8;

const std::vector<std::string> CommandHelp{
    "addFolder <id> <parent|root> <emoji> <name>     Add a folder/category node",
    "addTask <id> <parent|root> <emoji> <name>       Add an individual task node",
    "remove <id>                                      Remove a node and all descendants",
    "setDescription <id> <description>                Set text shown in the right pane",
    "setEmoji <id> <emoji>                            Change the marker for a node",
    "manualSelect [id]                                Enter arrow/mouse selection mode",
    "select <id>                                      Select a node without manual mode",
    "emojis                                           Show every allowed emoji marker",
    "commands                                         Show this command list",
    "list                                             Return to the tree view",
    "quit                                             Exit SentinelTasks",
    "",
    "Tip: enter a command requiring arguments by itself to open its GUI-style argument window."
};

struct DialogRect {
    int top{0};
    int left{0};
    int height{0};
    int width{0};
};

bool IsUtf8Continuation(unsigned char value) {
    return (value & 0xC0U) == 0x80U;
}

std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t");
    return value.substr(first, last - first + 1);
}

void DrawClipped(int row, int column, int width, const std::string& value) {
    if (width <= 0 || column < 0) return;
    mvaddnstr(row, column, value.c_str(), width);
}

std::vector<std::string> WrapText(const std::string& text, int width) {
    std::vector<std::string> lines;
    if (width <= 0) return lines;

    std::istringstream input(text);
    std::string word;
    std::string line;

    while (input >> word) {
        if (line.empty()) {
            line = word;
        } else if (static_cast<int>(line.size() + 1 + word.size()) <= width) {
            line += ' ';
            line += word;
        } else {
            lines.push_back(line);
            line = word;
        }
    }

    if (!line.empty()) lines.push_back(line);
    if (lines.empty() && !text.empty()) {
        lines.push_back(text.substr(0, static_cast<std::size_t>(width)));
    }
    return lines;
}

DialogRect CalculateDialogRect(std::size_t fieldCount) {
    int screenHeight = 0;
    int screenWidth = 0;
    getmaxyx(stdscr, screenHeight, screenWidth);

    DialogRect rect;
    rect.width = std::clamp(screenWidth * 3 / 5, 54, std::max(54, screenWidth - 4));
    rect.height = std::clamp(
        9 + static_cast<int>(fieldCount) * 2,
        11,
        std::max(11, screenHeight - 4)
    );
    rect.left = std::max(0, (screenWidth - rect.width) / 2);
    rect.top = std::max(0, (screenHeight - rect.height) / 2);
    return rect;
}

int DialogFieldRow(const DialogRect& rect, std::size_t fieldIndex) {
    return rect.top + 4 + static_cast<int>(fieldIndex) * 2;
}

} // namespace

int Application::Run() {
    std::setlocale(LC_ALL, "");

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS, nullptr);
    timeout(100);
    curs_set(1);

    while (running_) {
        HandleInput();
        Render();
    }

    endwin();
    return 0;
}

void Application::HandleInput() {
    const int key = getch();
    if (key == ERR) return;

    if (commandDialog_) {
        if (key == KEY_MOUSE) {
            MEVENT event{};
            if (getmouse(&event) == OK &&
                ((event.bstate & BUTTON1_CLICKED) != 0 || (event.bstate & BUTTON1_PRESSED) != 0)) {
                HandleCommandDialogMouse(event.x, event.y);
            }
            return;
        }
        HandleCommandDialogInput(key);
        return;
    }

    if (manualSelect_) {
        if (key == KEY_MOUSE) {
            HandleMouse();
            return;
        }
        if (key == KEY_UP) {
            MoveManualSelection(-1);
            return;
        }
        if (key == KEY_DOWN) {
            MoveManualSelection(1);
            return;
        }
        if (key == KEY_LEFT) {
            SelectParent();
            return;
        }
        if (key == KEY_RIGHT) {
            SelectFirstChild();
            return;
        }
        if (key == '\n' || key == KEY_ENTER) {
            status_ = selectedId_.empty() ? "No node selected." : "Selected: " + selectedId_;
            LeaveManualSelect();
            return;
        }
        if (key == 27) {
            status_ = "Manual selection ended.";
            LeaveManualSelect();
            return;
        }
        return;
    }

    const auto suggestions = BuildSuggestions();

    if (key == KEY_MOUSE) {
        HandleMouse();
        return;
    }
    if (key == '\t') {
        AcceptSuggestion(suggestions);
        ResetHistoryNavigation();
        return;
    }
    if (key == KEY_LEFT) {
        cursorPosition_ = PreviousUtf8Boundary(commandBuffer_, cursorPosition_);
        ResetSuggestionNavigation();
        return;
    }
    if (key == KEY_RIGHT) {
        cursorPosition_ = NextUtf8Boundary(commandBuffer_, cursorPosition_);
        ResetSuggestionNavigation();
        return;
    }
    if (key == KEY_UP) {
        if (navigatingSuggestions_ && !suggestions.empty()) {
            selectedSuggestion_ = selectedSuggestion_ == 0 ? suggestions.size() - 1 : selectedSuggestion_ - 1;
        } else {
            RecallPreviousCommand();
        }
        return;
    }
    if (key == KEY_DOWN) {
        if (navigatingSuggestions_ && !suggestions.empty()) {
            selectedSuggestion_ = (selectedSuggestion_ + 1) % suggestions.size();
        } else if (historyIndex_) {
            RecallNextCommand();
        } else if (!suggestions.empty()) {
            navigatingSuggestions_ = true;
            selectedSuggestion_ = 0;
        }
        return;
    }
    if (key == '\n' || key == KEY_ENTER) {
        if (navigatingSuggestions_ && !suggestions.empty()) {
            AcceptSuggestion(suggestions);
            return;
        }

        const std::string command = Trim(commandBuffer_);
        if (!command.empty()) {
            AddCommandToHistory(command);
            ExecuteCommand(command);
        }
        commandBuffer_.clear();
        cursorPosition_ = 0;
        ResetSuggestionNavigation();
        return;
    }
    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (cursorPosition_ > 0) {
            const std::size_t previous = PreviousUtf8Boundary(commandBuffer_, cursorPosition_);
            commandBuffer_.erase(previous, cursorPosition_ - previous);
            cursorPosition_ = previous;
        }
        ResetHistoryNavigation();
        ResetSuggestionNavigation();
        return;
    }

    if (key >= 32 && key <= 255) {
        commandBuffer_.insert(
            commandBuffer_.begin() + static_cast<std::ptrdiff_t>(cursorPosition_),
            static_cast<char>(key)
        );
        ++cursorPosition_;
        ResetHistoryNavigation();
        ResetSuggestionNavigation();
    }
}

void Application::HandleMouse() {
    MEVENT event{};
    if (getmouse(&event) != OK) return;
    if ((event.bstate & BUTTON1_CLICKED) == 0 && (event.bstate & BUTTON1_PRESSED) == 0) return;

    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)width;

    if (manualSelect_) {
        const int rowIndex = event.y - 2;
        if (rowIndex >= 0 && rowIndex < static_cast<int>(visibleRowIds_.size())) {
            selectedId_ = visibleRowIds_[static_cast<std::size_t>(rowIndex)];
            status_ = "Selected: " + selectedId_;
        }
        return;
    }

    if (event.y == height - 1) {
        const int textColumn = std::max(0, event.x - 2);
        cursorPosition_ = std::min(commandBuffer_.size(), static_cast<std::size_t>(textColumn));
        ResetHistoryNavigation();
        ResetSuggestionNavigation();
        return;
    }

    const auto suggestions = BuildSuggestions();
    if (suggestions.empty()) return;
    const int startRow = std::max(2, height - 2 - static_cast<int>(suggestions.size()));
    if (event.y < startRow || event.y >= startRow + static_cast<int>(suggestions.size())) return;

    selectedSuggestion_ = static_cast<std::size_t>(event.y - startRow);
    AcceptSuggestion(suggestions);
}

void Application::ExecuteCommand(const std::string& commandLine) {
    const auto tokens = Tokenize(commandLine);
    if (tokens.empty()) return;

    const std::string& command = tokens[0];
    infoLines_.clear();

    // Commands that need arguments can be entered by name alone. In that
    // case SentinelTasks opens a GUI-like ncurses form instead of reporting a
    // usage error. Supplying arguments inline keeps the original CLI path.
    if (tokens.size() == 1 && OpenCommandDialog(command)) {
        return;
    }

    if (command == "quit" || command == "exit") {
        running_ = false;
        return;
    }
    if (command == "commands") {
        infoLines_ = CommandHelp;
        status_ = "Available SentinelTasks commands.";
        return;
    }
    if (command == "emojis") {
        std::string line;
        for (const auto& emoji : TaskTree::AllowedEmojis()) {
            if (!line.empty()) line += ' ';
            line += emoji;
        }
        infoLines_ = {"Allowed emoji markers:", line};
        status_ = "All listed markers can be used by folders or tasks.";
        return;
    }
    if (command == "list") {
        status_ = "Tree view.";
        return;
    }
    if (command == "manualSelect") {
        if (tokens.size() > 2) {
            status_ = "Usage: manualSelect [id]";
            return;
        }
        EnterManualSelect(tokens.size() == 2 ? std::optional<std::string>{tokens[1]} : std::nullopt);
        return;
    }
    if (command == "select") {
        if (tokens.size() != 2 || !tree_.GetNode(tokens[1])) {
            status_ = "Usage: select <existing-id>";
            return;
        }
        selectedId_ = tokens[1];
        status_ = "Selected: " + selectedId_;
        return;
    }
    if (command == "remove") {
        if (tokens.size() != 2) {
            status_ = "Usage: remove <id>";
            return;
        }
        std::string error;
        if (!tree_.RemoveNode(tokens[1], error)) {
            status_ = error;
            return;
        }
        if (selectedId_ == tokens[1] || !tree_.GetNode(selectedId_)) selectedId_.clear();
        EnsureSelection();
        status_ = "Removed: " + tokens[1];
        return;
    }
    if (command == "setEmoji") {
        if (tokens.size() != 3) {
            status_ = "Usage: setEmoji <id> <emoji>";
            return;
        }
        std::string error;
        status_ = tree_.SetEmoji(tokens[1], tokens[2], error) ? "Emoji updated: " + tokens[1] : error;
        return;
    }
    if (command == "setDescription") {
        if (tokens.size() < 3) {
            status_ = "Usage: setDescription <id> <description>";
            return;
        }
        std::string error;
        const std::string description = JoinTokens(tokens, 2);
        status_ = tree_.SetDescription(tokens[1], description, error) ? "Description updated: " + tokens[1] : error;
        return;
    }
    if (command == "addFolder" || command == "addTask") {
        if (tokens.size() < 5) {
            status_ = "Usage: " + command + " <id> <parent|root> <emoji> <name>";
            return;
        }

        std::string error;
        const NodeKind kind = command == "addFolder" ? NodeKind::Folder : NodeKind::Task;
        const std::string name = JoinTokens(tokens, 4);

        if (!tree_.AddNode(kind, tokens[1], tokens[2], tokens[3], name, error)) {
            status_ = error;
            return;
        }

        selectedId_ = tokens[1];
        status_ = std::string(kind == NodeKind::Folder ? "Folder added: " : "Task added: ") + tokens[1];
        return;
    }

    status_ = "Unknown command: " + command + ". Type 'commands'.";
}

void Application::Render() {
    erase();
    EnsureSelection();
    RenderHeader();
    RenderTree();
    RenderDescriptionPane();

    if (!manualSelect_ && !commandDialog_) {
        const auto suggestions = BuildSuggestions();
        if (!suggestions.empty() && selectedSuggestion_ >= suggestions.size()) selectedSuggestion_ = 0;
        RenderSuggestions(suggestions);
    }

    RenderStatus();
    RenderCommandLine();

    if (commandDialog_) {
        RenderCommandDialog();
    }

    refresh();
}

void Application::RenderHeader() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)height;

    std::string title;
    if (commandDialog_) {
        title = "SentinelTasks | COMMAND ARGUMENT WINDOW";
    } else if (manualSelect_) {
        title = "SentinelTasks | MANUAL SELECT";
    } else {
        title = "SentinelTasks | Tree Task Planner";
    }

    DrawClipped(0, 0, std::max(0, width - 1), title);
    if (width > 1) mvhline(1, 0, ACS_HLINE, width - 1);
}

void Application::RenderTree() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);

    const int divider = std::clamp((width * 64) / 100, 40, std::max(40, width - 30));
    const int treeWidth = std::max(1, divider - 2);
    const int lastRow = std::max(2, height - 3);

    visibleRowIds_.clear();

    if (!infoLines_.empty()) {
        int row = 2;
        for (const auto& line : infoLines_) {
            if (row >= lastRow) break;
            DrawClipped(row++, 0, treeWidth, line);
        }
        return;
    }

    const auto visible = tree_.Flatten();
    int row = 2;

    for (const auto& entry : visible) {
        if (row >= lastRow || !entry.node) break;

        const TaskNode& node = *entry.node;
        const bool selected = node.id == selectedId_;
        const std::string kind = node.kind == NodeKind::Folder ? "[F] " : "[T] ";
        const std::string line = entry.connectorPrefix + node.emoji + " " + kind + node.name + "  {" + node.id + "}";

        if (selected) attron(A_REVERSE);
        DrawClipped(row, 0, treeWidth, line);
        if (selected) attroff(A_REVERSE);

        visibleRowIds_.push_back(node.id);
        ++row;
    }

    if (visible.empty()) {
        DrawClipped(3, 0, treeWidth, "No nodes yet. Use addFolder or addTask.");
    }
}

void Application::RenderDescriptionPane() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);

    const int divider = std::clamp((width * 64) / 100, 40, std::max(40, width - 30));
    if (divider >= width - 2) return;

    for (int row = 2; row < height - 2; ++row) {
        mvaddch(row, divider, ACS_VLINE);
    }

    const int column = divider + 2;
    const int paneWidth = std::max(1, width - column - 1);
    DrawClipped(2, column, paneWidth, "Description");
    if (paneWidth > 0) mvhline(3, column, ACS_HLINE, paneWidth);

    const TaskNode* node = tree_.GetNode(selectedId_);
    if (!node) {
        DrawClipped(5, column, paneWidth, "No task selected.");
        return;
    }

    DrawClipped(5, column, paneWidth, "ID: " + node->id);
    DrawClipped(6, column, paneWidth, "Type: " + std::string(node->kind == NodeKind::Folder ? "folder" : "task"));
    DrawClipped(7, column, paneWidth, "Marker: " + node->emoji);
    DrawClipped(9, column, paneWidth, node->name);

    const std::string description = node->description.empty() ? "(No description)" : node->description;
    const auto lines = WrapText(description, paneWidth);
    int row = 11;
    for (const auto& line : lines) {
        if (row >= height - 3) break;
        DrawClipped(row++, column, paneWidth, line);
    }
}

void Application::RenderSuggestions(const std::vector<Suggestion>& suggestions) {
    if (suggestions.empty()) return;

    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    const int startRow = std::max(2, height - 2 - static_cast<int>(suggestions.size()));

    for (std::size_t index = 0; index < suggestions.size(); ++index) {
        const bool selected = index == selectedSuggestion_;
        if (selected) attron(A_REVERSE);
        DrawClipped(
            startRow + static_cast<int>(index),
            0,
            std::max(0, width - 1),
            (selected ? "> " : "  ") + suggestions[index].label
        );
        if (selected) attroff(A_REVERSE);
    }
}

void Application::RenderStatus() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    if (height < 2) return;

    std::string line = status_;
    if (commandDialog_) {
        line += line.empty() ? "" : " | ";
        line += "Tab fields | Enter open/accept | Up/Down dropdown | F2 submit | Esc cancel | mouse supported";
    } else if (manualSelect_) {
        line += line.empty() ? "" : " | ";
        line += "Up/Down select | Left parent | Right child | click select | Enter/Esc finish";
    } else if (!commandBuffer_.empty()) {
        line += line.empty() ? "" : " | ";
        line += "Tab complete | Down suggestions | Up history | Left/Right cursor";
    }

    DrawClipped(height - 2, 0, std::max(0, width - 1), line);
}

void Application::RenderCommandLine() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    if (height < 1 || width < 1) return;

    const int row = height - 1;
    move(row, 0);
    clrtoeol();

    if (commandDialog_) {
        curs_set(0);
        DrawClipped(row, 0, std::max(0, width - 1), "> [argument window: " + commandDialog_->command + "]");
        return;
    }

    if (manualSelect_) {
        curs_set(0);
        DrawClipped(row, 0, std::max(0, width - 1), "> [manualSelect] " + selectedId_);
        return;
    }

    curs_set(1);
    DrawClipped(row, 0, 2, "> ");
    if (width > 2) DrawClipped(row, 2, width - 3, commandBuffer_);
    move(row, std::min(width - 1, static_cast<int>(cursorPosition_) + 2));
}

void Application::RenderCommandDialog() {
    if (!commandDialog_) return;

    CommandDialog& dialog = *commandDialog_;
    const DialogRect rect = CalculateDialogRect(dialog.fields.size());

    // Clear and frame a centered ncurses window-like surface.
    for (int row = rect.top; row < rect.top + rect.height; ++row) {
        move(row, rect.left);
        for (int column = 0; column < rect.width; ++column) addch(' ');
    }

    mvhline(rect.top, rect.left + 1, ACS_HLINE, std::max(0, rect.width - 2));
    mvhline(rect.top + rect.height - 1, rect.left + 1, ACS_HLINE, std::max(0, rect.width - 2));
    mvvline(rect.top + 1, rect.left, ACS_VLINE, std::max(0, rect.height - 2));
    mvvline(rect.top + 1, rect.left + rect.width - 1, ACS_VLINE, std::max(0, rect.height - 2));
    mvaddch(rect.top, rect.left, ACS_ULCORNER);
    mvaddch(rect.top, rect.left + rect.width - 1, ACS_URCORNER);
    mvaddch(rect.top + rect.height - 1, rect.left, ACS_LLCORNER);
    mvaddch(rect.top + rect.height - 1, rect.left + rect.width - 1, ACS_LRCORNER);

    DrawClipped(rect.top + 1, rect.left + 3, rect.width - 6, dialog.title);
    DrawClipped(
        rect.top + 2,
        rect.left + 3,
        rect.width - 6,
        "Fill arguments manually. CLI arguments remain supported."
    );

    const int labelWidth = std::min(18, std::max(10, rect.width / 4));
    const int inputColumn = rect.left + 3 + labelWidth;
    const int inputWidth = std::max(10, rect.width - labelWidth - 7);

    for (std::size_t index = 0; index < dialog.fields.size(); ++index) {
        DialogField& field = dialog.fields[index];
        const int row = DialogFieldRow(rect, index);
        if (row >= rect.top + rect.height - 4) break;

        const bool focused = dialog.focusedControl == index;
        DrawClipped(row, rect.left + 3, labelWidth - 1, field.label + ":");

        if (focused) attron(A_REVERSE);
        if (field.kind == DialogFieldKind::TextInput) {
            std::string display = "[ " + field.value;
            if (static_cast<int>(display.size()) < inputWidth - 1) {
                display += std::string(static_cast<std::size_t>(inputWidth - 1 - display.size()), ' ');
            }
            display += "]";
            DrawClipped(row, inputColumn, inputWidth, display);
        } else {
            std::string value = field.value.empty() ? "(no options)" : field.value;
            std::string display = "[ " + value + "  v";
            if (static_cast<int>(display.size()) < inputWidth - 1) {
                display += std::string(static_cast<std::size_t>(inputWidth - 1 - display.size()), ' ');
            }
            display += "]";
            DrawClipped(row, inputColumn, inputWidth, display);
        }
        if (focused) attroff(A_REVERSE);
    }

    const std::size_t submitIndex = dialog.fields.size();
    const std::size_t cancelIndex = dialog.fields.size() + 1;
    const int buttonRow = rect.top + rect.height - 3;
    const int submitColumn = rect.left + rect.width / 2 - 14;
    const int cancelColumn = rect.left + rect.width / 2 + 3;

    if (dialog.focusedControl == submitIndex) attron(A_REVERSE);
    DrawClipped(buttonRow, submitColumn, 12, "[ Submit ]");
    if (dialog.focusedControl == submitIndex) attroff(A_REVERSE);

    if (dialog.focusedControl == cancelIndex) attron(A_REVERSE);
    DrawClipped(buttonRow, cancelColumn, 12, "[ Cancel ]");
    if (dialog.focusedControl == cancelIndex) attroff(A_REVERSE);

    if (!dialog.validationMessage.empty()) {
        DrawClipped(
            rect.top + rect.height - 2,
            rect.left + 3,
            rect.width - 6,
            dialog.validationMessage
        );
    }

    // Draw an opened dropdown last so it behaves as an overlay above fields.
    if (dialog.focusedControl < dialog.fields.size()) {
        DialogField& field = dialog.fields[dialog.focusedControl];
        if (field.kind == DialogFieldKind::DropList && field.dropdownOpen) {
            const int fieldRow = DialogFieldRow(rect, dialog.focusedControl);
            const std::size_t optionCount = std::min(MaxDropdownRows, field.options.size());
            if (optionCount > 0) {
                std::size_t start = 0;
                if (field.selectedOption >= optionCount) {
                    start = field.selectedOption - optionCount + 1;
                }
                if (start + optionCount > field.options.size()) {
                    start = field.options.size() - optionCount;
                }

                for (std::size_t visible = 0; visible < optionCount; ++visible) {
                    const std::size_t optionIndex = start + visible;
                    const int optionRow = fieldRow + 1 + static_cast<int>(visible);
                    if (optionRow >= rect.top + rect.height - 2) break;

                    if (optionIndex == field.selectedOption) attron(A_REVERSE);
                    std::string option = "  " + field.options[optionIndex];
                    if (static_cast<int>(option.size()) < inputWidth) {
                        option += std::string(static_cast<std::size_t>(inputWidth - option.size()), ' ');
                    }
                    DrawClipped(optionRow, inputColumn, inputWidth, option);
                    if (optionIndex == field.selectedOption) attroff(A_REVERSE);
                }
            }
        }
    }

    // Put the real terminal cursor inside the currently focused text field.
    if (dialog.focusedControl < dialog.fields.size()) {
        DialogField& field = dialog.fields[dialog.focusedControl];
        if (field.kind == DialogFieldKind::TextInput) {
            curs_set(1);
            const int row = DialogFieldRow(rect, dialog.focusedControl);
            const int cursorColumn = inputColumn + 2 + static_cast<int>(field.cursor);
            move(row, std::min(inputColumn + inputWidth - 2, cursorColumn));
            return;
        }
    }

    curs_set(0);
}

std::vector<Application::Suggestion> Application::BuildSuggestions() const {
    std::vector<Suggestion> suggestions;
    if (commandBuffer_.empty()) return suggestions;

    const auto separator = commandBuffer_.find_first_of(" \t");
    if (separator != std::string::npos) return suggestions;

    std::string query = commandBuffer_;
    std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    struct Ranked {
        int score;
        CommandDefinition command;
    };
    std::vector<Ranked> ranked;

    for (const auto& command : Commands) {
        std::string candidate(command.name);
        std::transform(candidate.begin(), candidate.end(), candidate.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        const auto position = candidate.find(query);
        if (position == std::string::npos) continue;
        ranked.push_back({position == 0 ? 1000 - static_cast<int>(candidate.size()) : 500 - static_cast<int>(position), command});
    }

    std::sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.command.name < b.command.name;
    });

    for (std::size_t index = 0; index < ranked.size() && index < MaxSuggestions; ++index) {
        suggestions.push_back({
            std::string(ranked[index].command.name) + "  -  " + std::string(ranked[index].command.description),
            std::string(ranked[index].command.name) + " "
        });
    }

    return suggestions;
}

void Application::AcceptSuggestion(const std::vector<Suggestion>& suggestions) {
    if (suggestions.empty()) return;
    selectedSuggestion_ = std::min(selectedSuggestion_, suggestions.size() - 1);
    commandBuffer_ = suggestions[selectedSuggestion_].replacement;
    cursorPosition_ = commandBuffer_.size();
    ResetSuggestionNavigation();
}

void Application::ResetSuggestionNavigation() {
    selectedSuggestion_ = 0;
    navigatingSuggestions_ = false;
}

void Application::AddCommandToHistory(const std::string& command) {
    if (command.empty()) return;
    if (commandHistory_.empty() || commandHistory_.back() != command) {
        commandHistory_.push_back(command);
    }
    ResetHistoryNavigation();
}

void Application::RecallPreviousCommand() {
    if (commandHistory_.empty()) return;

    if (!historyIndex_) {
        commandBeforeHistory_ = commandBuffer_;
        historyIndex_ = commandHistory_.size() - 1;
    } else if (*historyIndex_ > 0) {
        --(*historyIndex_);
    }

    commandBuffer_ = commandHistory_[*historyIndex_];
    cursorPosition_ = commandBuffer_.size();
    ResetSuggestionNavigation();
}

void Application::RecallNextCommand() {
    if (!historyIndex_) return;

    if (*historyIndex_ + 1 < commandHistory_.size()) {
        ++(*historyIndex_);
        commandBuffer_ = commandHistory_[*historyIndex_];
    } else {
        commandBuffer_ = commandBeforeHistory_;
        ResetHistoryNavigation();
    }

    cursorPosition_ = commandBuffer_.size();
    ResetSuggestionNavigation();
}

void Application::ResetHistoryNavigation() {
    historyIndex_.reset();
    commandBeforeHistory_.clear();
}

void Application::EnterManualSelect(const std::optional<std::string>& requestedId) {
    if (tree_.Empty()) {
        status_ = "Cannot enter manualSelect: tree is empty.";
        return;
    }

    if (requestedId) {
        if (!tree_.GetNode(*requestedId)) {
            status_ = "Node ID does not exist: " + *requestedId;
            return;
        }
        selectedId_ = *requestedId;
    }

    EnsureSelection();
    manualSelect_ = true;
    status_ = "Manual selection mode.";
}

void Application::LeaveManualSelect() {
    manualSelect_ = false;
    curs_set(1);
}

void Application::MoveManualSelection(int delta) {
    const auto visible = tree_.Flatten();
    if (visible.empty()) return;

    std::size_t index = 0;
    for (std::size_t current = 0; current < visible.size(); ++current) {
        if (visible[current].node && visible[current].node->id == selectedId_) {
            index = current;
            break;
        }
    }

    if (delta < 0) {
        index = index == 0 ? visible.size() - 1 : index - 1;
    } else {
        index = (index + 1) % visible.size();
    }

    selectedId_ = visible[index].node->id;
    status_ = "Selected: " + selectedId_;
}

void Application::SelectParent() {
    if (const auto parent = tree_.ParentOf(selectedId_)) {
        selectedId_ = *parent;
        status_ = "Selected parent: " + selectedId_;
    }
}

void Application::SelectFirstChild() {
    if (const auto child = tree_.FirstChildOf(selectedId_)) {
        selectedId_ = *child;
        status_ = "Selected child: " + selectedId_;
    }
}

void Application::EnsureSelection() {
    if (!selectedId_.empty() && tree_.GetNode(selectedId_)) return;

    const auto visible = tree_.Flatten();
    selectedId_ = visible.empty() || !visible.front().node ? std::string{} : visible.front().node->id;
}

bool Application::OpenCommandDialog(const std::string& command) {
    CommandDialog dialog;
    dialog.command = command;
    dialog.title = "Command: " + command;

    const auto makeText = [](std::string label, std::string value = {}) {
        DialogField field;
        field.label = std::move(label);
        field.kind = DialogFieldKind::TextInput;
        field.value = std::move(value);
        field.cursor = field.value.size();
        return field;
    };

    const auto makeDrop = [&](std::string label, std::vector<std::string> options, std::string preferred = {}) {
        DialogField field;
        field.label = std::move(label);
        field.kind = DialogFieldKind::DropList;
        field.options = std::move(options);
        field.selectedOption = FindOptionIndex(field.options, preferred).value_or(0);
        if (!field.options.empty()) field.value = field.options[field.selectedOption];
        return field;
    };

    if (command == "addFolder" || command == "addTask") {
        dialog.fields.push_back(makeText("ID"));

        std::string preferredParent = "root";
        if (const TaskNode* selected = tree_.GetNode(selectedId_)) {
            if (selected->kind == NodeKind::Folder) preferredParent = selected->id;
            else if (!selected->parentId.empty()) preferredParent = selected->parentId;
        }
        dialog.fields.push_back(makeDrop("Parent", NodeIdOptions(true, true), preferredParent));

        const std::string defaultEmoji = command == "addFolder" ? "🟥" : "🔘";
        dialog.fields.push_back(makeDrop("Emoji", TaskTree::AllowedEmojis(), defaultEmoji));
        dialog.fields.push_back(makeText("Name"));
    } else if (command == "setDescription") {
        dialog.fields.push_back(makeDrop("Node", NodeIdOptions(false, false), selectedId_));
        std::string description;
        if (const TaskNode* node = tree_.GetNode(selectedId_)) description = node->description;
        dialog.fields.push_back(makeText("Description", description));
    } else if (command == "setEmoji") {
        dialog.fields.push_back(makeDrop("Node", NodeIdOptions(false, false), selectedId_));
        std::string currentEmoji;
        if (const TaskNode* node = tree_.GetNode(selectedId_)) currentEmoji = node->emoji;
        dialog.fields.push_back(makeDrop("Emoji", TaskTree::AllowedEmojis(), currentEmoji));
    } else if (command == "remove" || command == "select" || command == "manualSelect") {
        dialog.fields.push_back(makeDrop("Node", NodeIdOptions(false, false), selectedId_));
    } else {
        return false;
    }

    commandDialog_ = std::move(dialog);
    status_ = "Argument window opened for: " + command;
    ResetSuggestionNavigation();
    curs_set(0);
    return true;
}

void Application::CloseCommandDialog() {
    commandDialog_.reset();
    curs_set(1);
}

void Application::HandleCommandDialogInput(int key) {
    if (!commandDialog_) return;
    CommandDialog& dialog = *commandDialog_;

    if (key == 27) {
        if (dialog.focusedControl < dialog.fields.size() &&
            dialog.fields[dialog.focusedControl].dropdownOpen) {
            CloseFocusedDropList(false);
        } else {
            status_ = "Argument window cancelled.";
            CloseCommandDialog();
        }
        return;
    }

    if (key == KEY_F(2)) {
        SubmitCommandDialog();
        return;
    }

    if (key == '\t') {
        if (dialog.focusedControl < dialog.fields.size()) {
            dialog.fields[dialog.focusedControl].dropdownOpen = false;
        }
        MoveDialogFocus(1);
        return;
    }
#ifdef KEY_BTAB
    if (key == KEY_BTAB) {
        if (dialog.focusedControl < dialog.fields.size()) {
            dialog.fields[dialog.focusedControl].dropdownOpen = false;
        }
        MoveDialogFocus(-1);
        return;
    }
#endif

    const std::size_t submitIndex = dialog.fields.size();
    const std::size_t cancelIndex = dialog.fields.size() + 1;

    if (dialog.focusedControl == submitIndex) {
        if (key == '\n' || key == KEY_ENTER || key == ' ') SubmitCommandDialog();
        else if (key == KEY_LEFT) MoveDialogFocus(-1);
        else if (key == KEY_RIGHT) MoveDialogFocus(1);
        return;
    }

    if (dialog.focusedControl == cancelIndex) {
        if (key == '\n' || key == KEY_ENTER || key == ' ') {
            status_ = "Argument window cancelled.";
            CloseCommandDialog();
        } else if (key == KEY_LEFT) MoveDialogFocus(-1);
        else if (key == KEY_RIGHT) MoveDialogFocus(1);
        return;
    }

    if (dialog.focusedControl >= dialog.fields.size()) return;
    DialogField& field = dialog.fields[dialog.focusedControl];

    if (field.kind == DialogFieldKind::DropList) {
        if (field.options.empty()) return;

        if (key == KEY_UP) {
            field.dropdownOpen = true;
            field.selectedOption = field.selectedOption == 0 ? field.options.size() - 1 : field.selectedOption - 1;
            field.value = field.options[field.selectedOption];
            return;
        }
        if (key == KEY_DOWN) {
            field.dropdownOpen = true;
            field.selectedOption = (field.selectedOption + 1) % field.options.size();
            field.value = field.options[field.selectedOption];
            return;
        }
        if (key == '\n' || key == KEY_ENTER || key == ' ') {
            if (field.dropdownOpen) CloseFocusedDropList(true);
            else OpenFocusedDropList();
            return;
        }
        if (key == KEY_LEFT) {
            field.dropdownOpen = false;
            MoveDialogFocus(-1);
            return;
        }
        if (key == KEY_RIGHT) {
            field.dropdownOpen = false;
            MoveDialogFocus(1);
            return;
        }
        return;
    }

    if (key == KEY_LEFT) {
        field.cursor = PreviousUtf8Boundary(field.value, field.cursor);
        return;
    }
    if (key == KEY_RIGHT) {
        field.cursor = NextUtf8Boundary(field.value, field.cursor);
        return;
    }
    if (key == KEY_HOME) {
        field.cursor = 0;
        return;
    }
    if (key == KEY_END) {
        field.cursor = field.value.size();
        return;
    }
    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (field.cursor > 0) {
            const std::size_t previous = PreviousUtf8Boundary(field.value, field.cursor);
            field.value.erase(previous, field.cursor - previous);
            field.cursor = previous;
        }
        return;
    }
    if (key == '\n' || key == KEY_ENTER) {
        MoveDialogFocus(1);
        return;
    }
    if (key >= 32 && key <= 255) {
        field.value.insert(
            field.value.begin() + static_cast<std::ptrdiff_t>(field.cursor),
            static_cast<char>(key)
        );
        ++field.cursor;
        return;
    }
}

void Application::HandleCommandDialogMouse(int mouseX, int mouseY) {
    if (!commandDialog_) return;
    CommandDialog& dialog = *commandDialog_;
    const DialogRect rect = CalculateDialogRect(dialog.fields.size());

    const std::size_t submitIndex = dialog.fields.size();
    const std::size_t cancelIndex = dialog.fields.size() + 1;
    const int buttonRow = rect.top + rect.height - 3;
    const int submitColumn = rect.left + rect.width / 2 - 14;
    const int cancelColumn = rect.left + rect.width / 2 + 3;

    if (mouseY == buttonRow && mouseX >= submitColumn && mouseX < submitColumn + 12) {
        dialog.focusedControl = submitIndex;
        SubmitCommandDialog();
        return;
    }
    if (mouseY == buttonRow && mouseX >= cancelColumn && mouseX < cancelColumn + 12) {
        dialog.focusedControl = cancelIndex;
        status_ = "Argument window cancelled.";
        CloseCommandDialog();
        return;
    }

    const int labelWidth = std::min(18, std::max(10, rect.width / 4));
    const int inputColumn = rect.left + 3 + labelWidth;
    const int inputWidth = std::max(10, rect.width - labelWidth - 7);

    // Check the currently opened drop-list overlay first.
    if (dialog.focusedControl < dialog.fields.size()) {
        DialogField& openField = dialog.fields[dialog.focusedControl];
        if (openField.kind == DialogFieldKind::DropList && openField.dropdownOpen && !openField.options.empty()) {
            const int fieldRow = DialogFieldRow(rect, dialog.focusedControl);
            const std::size_t optionCount = std::min(MaxDropdownRows, openField.options.size());
            std::size_t start = 0;
            if (openField.selectedOption >= optionCount) start = openField.selectedOption - optionCount + 1;
            if (start + optionCount > openField.options.size()) start = openField.options.size() - optionCount;

            if (mouseX >= inputColumn && mouseX < inputColumn + inputWidth &&
                mouseY >= fieldRow + 1 && mouseY < fieldRow + 1 + static_cast<int>(optionCount)) {
                const std::size_t option = start + static_cast<std::size_t>(mouseY - fieldRow - 1);
                if (option < openField.options.size()) {
                    openField.selectedOption = option;
                    openField.value = openField.options[option];
                    openField.dropdownOpen = false;
                }
                return;
            }
        }
    }

    for (std::size_t index = 0; index < dialog.fields.size(); ++index) {
        const int row = DialogFieldRow(rect, index);
        if (mouseY != row || mouseX < inputColumn || mouseX >= inputColumn + inputWidth) continue;

        dialog.focusedControl = index;
        DialogField& field = dialog.fields[index];
        if (field.kind == DialogFieldKind::DropList) {
            field.dropdownOpen = !field.dropdownOpen;
        } else {
            const int relative = std::max(0, mouseX - inputColumn - 2);
            field.cursor = std::min(field.value.size(), static_cast<std::size_t>(relative));
            while (field.cursor > 0 && field.cursor < field.value.size() &&
                   IsUtf8Continuation(static_cast<unsigned char>(field.value[field.cursor]))) {
                --field.cursor;
            }
        }
        return;
    }
}

void Application::MoveDialogFocus(int delta) {
    if (!commandDialog_) return;
    CommandDialog& dialog = *commandDialog_;
    const std::size_t controlCount = dialog.fields.size() + 2;
    if (controlCount == 0) return;

    if (delta < 0) {
        dialog.focusedControl = dialog.focusedControl == 0 ? controlCount - 1 : dialog.focusedControl - 1;
    } else {
        dialog.focusedControl = (dialog.focusedControl + 1) % controlCount;
    }
}

void Application::OpenFocusedDropList() {
    if (!commandDialog_) return;
    CommandDialog& dialog = *commandDialog_;
    if (dialog.focusedControl >= dialog.fields.size()) return;
    DialogField& field = dialog.fields[dialog.focusedControl];
    if (field.kind != DialogFieldKind::DropList || field.options.empty()) return;
    field.dropdownOpen = true;
}

void Application::CloseFocusedDropList(bool acceptSelection) {
    if (!commandDialog_) return;
    CommandDialog& dialog = *commandDialog_;
    if (dialog.focusedControl >= dialog.fields.size()) return;
    DialogField& field = dialog.fields[dialog.focusedControl];
    if (field.kind != DialogFieldKind::DropList) return;

    if (acceptSelection && !field.options.empty()) {
        field.selectedOption = std::min(field.selectedOption, field.options.size() - 1);
        field.value = field.options[field.selectedOption];
    }
    field.dropdownOpen = false;
}

bool Application::SubmitCommandDialog() {
    if (!commandDialog_) return false;
    CommandDialog& dialog = *commandDialog_;

    for (const DialogField& field : dialog.fields) {
        if (field.value.empty()) {
            dialog.validationMessage = "Required field is empty: " + field.label;
            return false;
        }
    }

    const std::string commandLine = BuildDialogCommand();
    if (commandLine.empty()) {
        dialog.validationMessage = "Could not build command arguments.";
        return false;
    }

    const std::string commandName = dialog.command;
    CloseCommandDialog();
    AddCommandToHistory(commandLine);
    ExecuteCommand(commandLine);
    status_ += status_.empty() ? "" : " | ";
    status_ += "Submitted via argument window: " + commandName;
    return true;
}

std::string Application::BuildDialogCommand() const {
    if (!commandDialog_) return {};
    const CommandDialog& dialog = *commandDialog_;
    std::string result = dialog.command;

    for (const DialogField& field : dialog.fields) {
        result += ' ';
        result += QuoteArgument(field.value);
    }

    return result;
}

std::vector<std::string> Application::NodeIdOptions(bool foldersOnly, bool includeRoot) const {
    std::vector<std::string> options;
    if (includeRoot) options.push_back("root");

    for (const auto& visible : tree_.Flatten()) {
        if (!visible.node) continue;
        if (foldersOnly && visible.node->kind != NodeKind::Folder) continue;
        options.push_back(visible.node->id);
    }

    return options;
}

std::optional<std::size_t> Application::FindOptionIndex(
    const std::vector<std::string>& options,
    const std::string& value
) const {
    const auto iterator = std::find(options.begin(), options.end(), value);
    if (iterator == options.end()) return std::nullopt;
    return static_cast<std::size_t>(std::distance(options.begin(), iterator));
}

std::string Application::QuoteArgument(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (const char character : value) {
        if (character == '\\' || character == '"') escaped.push_back('\\');
        escaped.push_back(character);
    }
    escaped.push_back('"');
    return escaped;
}

std::vector<std::string> Application::Tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool quoted = false;
    bool escaping = false;

    for (const char character : line) {
        if (escaping) {
            current.push_back(character);
            escaping = false;
            continue;
        }
        if (character == '\\') {
            escaping = true;
            continue;
        }
        if (character == '"') {
            quoted = !quoted;
            continue;
        }
        if (!quoted && std::isspace(static_cast<unsigned char>(character))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(character);
    }

    if (escaping) current.push_back('\\');
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

std::string Application::JoinTokens(const std::vector<std::string>& tokens, std::size_t start) {
    std::string result;
    for (std::size_t index = start; index < tokens.size(); ++index) {
        if (!result.empty()) result += ' ';
        result += tokens[index];
    }
    return result;
}

std::size_t Application::PreviousUtf8Boundary(const std::string& text, std::size_t position) {
    if (position == 0) return 0;
    std::size_t result = std::min(position, text.size()) - 1;
    while (result > 0 && IsUtf8Continuation(static_cast<unsigned char>(text[result]))) --result;
    return result;
}

std::size_t Application::NextUtf8Boundary(const std::string& text, std::size_t position) {
    if (position >= text.size()) return text.size();
    std::size_t result = position + 1;
    while (result < text.size() && IsUtf8Continuation(static_cast<unsigned char>(text[result]))) ++result;
    return result;
}
