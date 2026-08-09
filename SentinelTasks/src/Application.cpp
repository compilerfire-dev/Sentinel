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
    "quit                                             Exit SentinelTasks"
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
    if (lines.empty() && !text.empty()) lines.push_back(text.substr(0, static_cast<std::size_t>(width)));
    return lines;
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

    // getch() returns UTF-8 input as bytes on common ncurses terminals. Accept
    // the full byte range so emoji can be pasted/typed as command arguments.
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

    if (!manualSelect_) {
        const auto suggestions = BuildSuggestions();
        if (!suggestions.empty() && selectedSuggestion_ >= suggestions.size()) selectedSuggestion_ = 0;
        RenderSuggestions(suggestions);
    }

    RenderStatus();
    RenderCommandLine();
    refresh();
}

void Application::RenderHeader() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)height;

    const std::string title = manualSelect_
        ? "SentinelTasks | MANUAL SELECT"
        : "SentinelTasks | Tree Task Planner";

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
    if (manualSelect_) {
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
