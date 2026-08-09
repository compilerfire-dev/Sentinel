#include "Application.hpp"

#include <algorithm>
#include <array>
#include <clocale>
#include <cctype>
#include <limits>
#include <ncurses.h>
#include <regex>
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
    {"defineColor", "define a named RGB color"},
    {"color", "color default rows or one node"},
    {"manualSelect", "select nodes with arrows/mouse"},
    {"select", "select a node by ID"},
    {"commands", "show all commands"},
    {"list", "return to the tree view"},
    {"quit", "exit SentinelTasks"}
}};

constexpr std::size_t MaxSuggestions = 6;
constexpr short DefaultTreePair = 1;
constexpr short FirstNodePair = 2;

const std::vector<std::string> CommandHelp{
    "addFolder <id> <parent|root> <name>            Add a folder/category node",
    "addTask <id> <parent|root> <name>              Add an individual task node",
    "remove <id>                                     Remove a node and descendants",
    "setDescription <id> <description>               Set text shown in the right pane",
    "defineColor <name> rgb(r,g,b)                   Define/update a named color",
    "color <fg> bg <bg>                              Set default tree-row colors",
    "color <node-id> <fg> bg <bg>                    Set colors for one folder/task",
    "manualSelect [id]                               Enter arrow/mouse selection mode",
    "select <id>                                     Select a node by ID",
    "commands                                        Show this command list",
    "list                                            Return to tree view",
    "quit                                            Exit SentinelTasks",
    "",
    "Colors accept built-in names, defineColor names, or rgb(r,g,b).",
    "Enter an argument-taking command by itself to open its argument window."
};

const std::unordered_map<std::string, RgbColor> BuiltInColors{
    {"black", {0, 0, 0}},
    {"red", {205, 49, 49}},
    {"green", {13, 188, 121}},
    {"yellow", {229, 229, 16}},
    {"blue", {36, 114, 200}},
    {"magenta", {188, 63, 188}},
    {"cyan", {17, 168, 205}},
    {"white", {229, 229, 229}},
    {"brightblack", {102, 102, 102}},
    {"gray", {102, 102, 102}},
    {"grey", {102, 102, 102}},
    {"brightred", {241, 76, 76}},
    {"brightgreen", {35, 209, 139}},
    {"brightyellow", {245, 245, 67}},
    {"brightblue", {59, 142, 234}},
    {"brightmagenta", {214, 112, 214}},
    {"brightcyan", {41, 184, 219}},
    {"brightwhite", {255, 255, 255}}
};

struct DialogRect {
    int top{};
    int left{};
    int height{};
    int width{};
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

std::string NormalizeColorName(std::string value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char character : value) {
        if (std::isalnum(character)) {
            result.push_back(static_cast<char>(std::tolower(character)));
        }
    }
    return result;
}

std::optional<RgbColor> ParseRgbExpression(const std::string& value) {
    static const std::regex pattern(
        R"re(^\s*(?:rgb|rpg)\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*\)\s*$)re",
        std::regex::icase
    );

    std::smatch match;
    if (!std::regex_match(value, match, pattern)) return std::nullopt;

    const int red = std::stoi(match[1].str());
    const int green = std::stoi(match[2].str());
    const int blue = std::stoi(match[3].str());
    if (red > 255 || green > 255 || blue > 255) return std::nullopt;
    return RgbColor{red, green, blue};
}

void DrawClipped(int row, int column, int width, const std::string& value) {
    if (width > 0 && column >= 0) mvaddnstr(row, column, value.c_str(), width);
}

std::vector<std::string> WrapText(const std::string& text, int width) {
    std::vector<std::string> lines;
    if (width <= 0) return lines;

    std::istringstream input(text);
    std::string word;
    std::string line;
    while (input >> word) {
        if (line.empty()) line = word;
        else if (static_cast<int>(line.size() + word.size() + 1) <= width) line += " " + word;
        else {
            lines.push_back(line);
            line = word;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

DialogRect CalculateDialogRect(std::size_t fieldCount) {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);

    DialogRect rect;
    rect.width = std::clamp(width * 3 / 5, 54, std::max(54, width - 4));
    rect.height = std::clamp(9 + static_cast<int>(fieldCount) * 2, 11, std::max(11, height - 4));
    rect.left = std::max(0, (width - rect.width) / 2);
    rect.top = std::max(0, (height - rect.height) / 2);
    return rect;
}

int DialogFieldRow(const DialogRect& rect, std::size_t fieldIndex) {
    return rect.top + 4 + static_cast<int>(fieldIndex) * 2;
}

short NearestBasicColor(const RgbColor& color) {
    struct BasicColor {
        short index;
        int red;
        int green;
        int blue;
    };

    constexpr std::array<BasicColor, 8> colors{{
        {COLOR_BLACK, 0, 0, 0},
        {COLOR_RED, 255, 0, 0},
        {COLOR_GREEN, 0, 255, 0},
        {COLOR_YELLOW, 255, 255, 0},
        {COLOR_BLUE, 0, 0, 255},
        {COLOR_MAGENTA, 255, 0, 255},
        {COLOR_CYAN, 0, 255, 255},
        {COLOR_WHITE, 255, 255, 255}
    }};

    long bestDistance = std::numeric_limits<long>::max();
    short best = COLOR_WHITE;
    for (const auto& candidate : colors) {
        const long dr = color.red - candidate.red;
        const long dg = color.green - candidate.green;
        const long db = color.blue - candidate.blue;
        const long distance = dr * dr + dg * dg + db * db;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate.index;
        }
    }
    return best;
}

short NearestTerminalColor(const RgbColor& color) {
    if (!has_colors() || COLORS <= 0) return NearestBasicColor(color);

    const int colorLimit = std::min(
        COLORS,
        static_cast<int>(std::numeric_limits<short>::max()) + 1
    );

    long bestDistance = std::numeric_limits<long>::max();
    short best = NearestBasicColor(color);
    bool found = false;

    for (int index = 0; index < colorLimit; ++index) {
        short red = 0;
        short green = 0;
        short blue = 0;
        if (color_content(static_cast<short>(index), &red, &green, &blue) == ERR) continue;

        const int candidateRed = static_cast<int>(red) * 255 / 1000;
        const int candidateGreen = static_cast<int>(green) * 255 / 1000;
        const int candidateBlue = static_cast<int>(blue) * 255 / 1000;
        const long dr = color.red - candidateRed;
        const long dg = color.green - candidateGreen;
        const long db = color.blue - candidateBlue;
        const long distance = dr * dr + dg * dg + db * db;

        if (!found || distance < bestDistance) {
            found = true;
            bestDistance = distance;
            best = static_cast<short>(index);
        }
    }
    return best;
}

bool InitializeColorPair(short pair, const RgbColor& foreground, const RgbColor& background) {
    if (!has_colors() || pair <= 0 || pair >= COLOR_PAIRS) return false;
    return init_pair(pair, NearestTerminalColor(foreground), NearestTerminalColor(background)) != ERR;
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

    if (has_colors()) {
        start_color();
        InitializeColorPair(DefaultTreePair, treeDisplaySettings_.foreground, treeDisplaySettings_.background);
    }

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
            if (getmouse(&event) == OK) HandleCommandDialogMouse(event.x, event.y);
        } else {
            HandleCommandDialogInput(key);
        }
        return;
    }

    if (manualSelect_) {
        if (key == KEY_MOUSE) HandleMouse();
        else if (key == KEY_UP) MoveManualSelection(-1);
        else if (key == KEY_DOWN) MoveManualSelection(1);
        else if (key == KEY_LEFT) SelectParent();
        else if (key == KEY_RIGHT) SelectFirstChild();
        else if (key == '\n' || key == KEY_ENTER || key == 27) LeaveManualSelect();
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
        return;
    }
    if (key == KEY_RIGHT) {
        cursorPosition_ = NextUtf8Boundary(commandBuffer_, cursorPosition_);
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
            const auto previous = PreviousUtf8Boundary(commandBuffer_, cursorPosition_);
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
        const int index = event.y - 2;
        if (index >= 0 && index < static_cast<int>(visibleRowIds_.size())) {
            selectedId_ = visibleRowIds_[static_cast<std::size_t>(index)];
        }
        return;
    }

    if (event.y == height - 1) {
        cursorPosition_ = std::min(
            commandBuffer_.size(),
            static_cast<std::size_t>(std::max(0, event.x - 2))
        );
        return;
    }

    const auto suggestions = BuildSuggestions();
    if (suggestions.empty()) return;
    const int start = std::max(2, height - 2 - static_cast<int>(suggestions.size()));
    if (event.y >= start && event.y < start + static_cast<int>(suggestions.size())) {
        selectedSuggestion_ = static_cast<std::size_t>(event.y - start);
        AcceptSuggestion(suggestions);
    }
}

void Application::ExecuteCommand(const std::string& line) {
    const auto tokens = Tokenize(line);
    if (tokens.empty()) return;

    const std::string& command = tokens[0];
    infoLines_.clear();

    if (tokens.size() == 1 && OpenCommandDialog(command)) return;

    if (command == "quit" || command == "exit") {
        running_ = false;
        return;
    }
    if (command == "commands") {
        infoLines_ = CommandHelp;
        status_ = "Available SentinelTasks commands.";
        return;
    }
    if (command == "list") {
        status_ = "Tree view.";
        return;
    }
    if (command == "defineColor") {
        if (tokens.size() != 3) {
            status_ = "Usage: defineColor <name> rgb(r,g,b)";
            return;
        }
        DefineColor(tokens[1], tokens[2]);
        return;
    }
    if (command == "color") {
        ApplyColorCommand(tokens);
        return;
    }
    if (command == "manualSelect") {
        if (tokens.size() > 2) status_ = "Usage: manualSelect [id]";
        else EnterManualSelect(tokens.size() == 2 ? std::optional<std::string>{tokens[1]} : std::nullopt);
        return;
    }
    if (command == "select") {
        if (tokens.size() != 2 || !tree_.GetNode(tokens[1])) {
            status_ = "Usage: select <existing-id>";
        } else {
            selectedId_ = tokens[1];
            status_ = "Selected: " + selectedId_;
        }
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
        } else {
            if (!tree_.GetNode(selectedId_)) selectedId_.clear();
            EnsureSelection();
            status_ = "Removed: " + tokens[1];
        }
        return;
    }
    if (command == "setDescription") {
        if (tokens.size() < 3) {
            status_ = "Usage: setDescription <id> <description>";
            return;
        }
        std::string error;
        status_ = tree_.SetDescription(tokens[1], JoinTokens(tokens, 2), error)
            ? "Description updated: " + tokens[1]
            : error;
        return;
    }
    if (command == "addFolder" || command == "addTask") {
        if (tokens.size() < 4) {
            status_ = "Usage: " + command + " <id> <parent|root> <name>";
            return;
        }
        std::string error;
        const NodeKind kind = command == "addFolder" ? NodeKind::Folder : NodeKind::Task;
        if (!tree_.AddNode(kind, tokens[1], tokens[2], JoinTokens(tokens, 3), error)) {
            status_ = error;
        } else {
            selectedId_ = tokens[1];
            status_ = std::string(kind == NodeKind::Folder ? "Folder added: " : "Task added: ") + tokens[1];
        }
        return;
    }

    status_ = "Unknown command: " + command + ". Type 'commands'.";
}

std::optional<RgbColor> Application::ResolveColor(const std::string& value) const {
    const auto custom = definedColors_.find(value);
    if (custom != definedColors_.end()) return custom->second;

    const auto builtIn = BuiltInColors.find(NormalizeColorName(value));
    if (builtIn != BuiltInColors.end()) return builtIn->second;

    return ParseRgbExpression(value);
}

bool Application::DefineColor(const std::string& id, const std::string& rgbExpression) {
    if (id.empty()) {
        status_ = "Color name cannot be empty.";
        return false;
    }
    const auto color = ParseRgbExpression(rgbExpression);
    if (!color) {
        status_ = "Usage: defineColor <name> rgb(r,g,b), channels 0-255.";
        return false;
    }
    definedColors_[id] = *color;
    status_ = "Color defined: " + id;
    return true;
}

bool Application::ApplyColorCommand(const std::vector<std::string>& tokens) {
    if (tokens.size() == 4 && tokens[2] == "bg") {
        const auto foreground = ResolveColor(tokens[1]);
        const auto background = ResolveColor(tokens[3]);
        if (!foreground || !background) {
            status_ = "Unknown color. Use built-in names, defineColor names, or rgb(r,g,b).";
            return false;
        }
        treeDisplaySettings_.foreground = *foreground;
        treeDisplaySettings_.background = *background;
        if (has_colors()) InitializeColorPair(DefaultTreePair, *foreground, *background);
        status_ = "Default tree-row colors updated.";
        return true;
    }

    if (tokens.size() == 5 && tokens[3] == "bg") {
        const auto foreground = ResolveColor(tokens[2]);
        const auto background = ResolveColor(tokens[4]);
        if (!foreground || !background) {
            status_ = "Unknown color. Use built-in names, defineColor names, or rgb(r,g,b).";
            return false;
        }
        std::string error;
        if (!tree_.SetColor(tokens[1], *foreground, *background, error)) {
            status_ = error;
            return false;
        }
        status_ = "Node colors updated: " + tokens[1];
        return true;
    }

    status_ = "Usage: color <fg> bg <bg> OR color <node-id> <fg> bg <bg>";
    return false;
}

void Application::Render() {
    erase();
    attr_set(A_NORMAL, 0, nullptr);
    EnsureSelection();
    RenderHeader();
    RenderTree();
    RenderDescriptionPane();
    if (!manualSelect_ && !commandDialog_) RenderSuggestions(BuildSuggestions());
    RenderStatus();
    RenderCommandLine();
    if (commandDialog_) RenderCommandDialog();
    refresh();
}

void Application::RenderHeader() {
    attr_set(A_NORMAL, 0, nullptr);
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)height;

    const std::string title = commandDialog_
        ? "SentinelTasks | COMMAND ARGUMENT WINDOW"
        : manualSelect_
            ? "SentinelTasks | MANUAL SELECT"
            : "SentinelTasks | Tree Task Planner";

    DrawClipped(0, 0, std::max(0, width - 1), title);
    if (width > 1) mvhline(1, 0, ACS_HLINE, width - 1);
}

void Application::RenderTree() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);

    const int divider = std::clamp(width * 64 / 100, 40, std::max(40, width - 30));
    const int treeWidth = std::max(1, divider - 2);
    const int lastRow = std::max(2, height - 3);

    visibleRowIds_.clear();
    if (!infoLines_.empty()) {
        attr_set(A_NORMAL, 0, nullptr);
        int row = 2;
        for (const auto& line : infoLines_) {
            if (row >= lastRow) break;
            DrawClipped(row++, 0, treeWidth, line);
        }
        return;
    }

    const auto visible = tree_.Flatten();
    int row = 2;
    std::size_t visibleIndex = 0;

    for (const auto& entry : visible) {
        if (row >= lastRow || !entry.node) break;
        const TaskNode& node = *entry.node;
        const bool selected = node.id == selectedId_;

        short pair = DefaultTreePair;
        if (has_colors() && node.foregroundColor && node.backgroundColor) {
            const short candidate = static_cast<short>(FirstNodePair + visibleIndex);
            if (candidate > 0 && candidate < COLOR_PAIRS &&
                InitializeColorPair(candidate, *node.foregroundColor, *node.backgroundColor)) {
                pair = candidate;
            }
        }

        if (has_colors()) attr_set(selected ? A_REVERSE : A_NORMAL, pair, nullptr);
        else if (selected) attron(A_REVERSE);

        const std::string kind = node.kind == NodeKind::Folder ? "[F] " : "[T] ";
        const std::string text = entry.connectorPrefix + kind + node.name + "  {" + node.id + "}";
        DrawClipped(row, 0, treeWidth, text);

        attr_set(A_NORMAL, 0, nullptr);
        visibleRowIds_.push_back(node.id);
        ++row;
        ++visibleIndex;
    }

    if (visible.empty()) {
        attr_set(A_NORMAL, 0, nullptr);
        DrawClipped(3, 0, treeWidth, "No nodes yet. Use addFolder or addTask.");
    }
}

void Application::RenderDescriptionPane() {
    attr_set(A_NORMAL, 0, nullptr);
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);

    const int divider = std::clamp(width * 64 / 100, 40, std::max(40, width - 30));
    if (divider >= width - 2) return;

    for (int row = 2; row < height - 2; ++row) mvaddch(row, divider, ACS_VLINE);

    const int column = divider + 2;
    const int paneWidth = std::max(1, width - column - 1);
    DrawClipped(2, column, paneWidth, "Description");
    mvhline(3, column, ACS_HLINE, paneWidth);

    const TaskNode* node = tree_.GetNode(selectedId_);
    if (!node) {
        DrawClipped(5, column, paneWidth, "No task selected.");
        return;
    }

    DrawClipped(5, column, paneWidth, "ID: " + node->id);
    DrawClipped(6, column, paneWidth, "Type: " + std::string(node->kind == NodeKind::Folder ? "folder" : "task"));
    DrawClipped(8, column, paneWidth, node->name);

    const auto lines = WrapText(node->description.empty() ? "(No description)" : node->description, paneWidth);
    int row = 10;
    for (const auto& line : lines) {
        if (row >= height - 3) break;
        DrawClipped(row++, column, paneWidth, line);
    }
}

void Application::RenderSuggestions(const std::vector<Suggestion>& suggestions) {
    if (suggestions.empty()) return;
    attr_set(A_NORMAL, 0, nullptr);

    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    const int start = std::max(2, height - 2 - static_cast<int>(suggestions.size()));

    for (std::size_t index = 0; index < suggestions.size(); ++index) {
        if (index == selectedSuggestion_) attron(A_REVERSE);
        DrawClipped(
            start + static_cast<int>(index),
            0,
            width - 1,
            (index == selectedSuggestion_ ? "> " : "  ") + suggestions[index].label
        );
        if (index == selectedSuggestion_) attroff(A_REVERSE);
    }
}

void Application::RenderStatus() {
    attr_set(A_NORMAL, 0, nullptr);
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);

    std::string line = status_;
    if (commandDialog_) line += " | Tab fields | Enter choose | F2 submit | Esc cancel";
    else if (manualSelect_) line += " | Up/Down select | Left parent | Right child | Enter/Esc finish";
    DrawClipped(height - 2, 0, width - 1, line);
}

void Application::RenderCommandLine() {
    attr_set(A_NORMAL, 0, nullptr);
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    move(height - 1, 0);
    clrtoeol();

    if (commandDialog_) {
        curs_set(0);
        DrawClipped(height - 1, 0, width - 1, "> [argument window: " + commandDialog_->command + "]");
        return;
    }
    if (manualSelect_) {
        curs_set(0);
        DrawClipped(height - 1, 0, width - 1, "> [manualSelect] " + selectedId_);
        return;
    }

    curs_set(1);
    DrawClipped(height - 1, 0, 2, "> ");
    DrawClipped(height - 1, 2, width - 3, commandBuffer_);
    move(height - 1, std::min(width - 1, static_cast<int>(cursorPosition_) + 2));
}

void Application::RenderCommandDialog() {
    if (!commandDialog_) return;
    attr_set(A_NORMAL, 0, nullptr);

    auto& dialog = *commandDialog_;
    const auto rect = CalculateDialogRect(dialog.fields.size());

    for (int y = rect.top; y < rect.top + rect.height; ++y) {
        move(y, rect.left);
        for (int x = 0; x < rect.width; ++x) addch(' ');
    }

    mvhline(rect.top, rect.left + 1, ACS_HLINE, rect.width - 2);
    mvhline(rect.top + rect.height - 1, rect.left + 1, ACS_HLINE, rect.width - 2);
    mvvline(rect.top + 1, rect.left, ACS_VLINE, rect.height - 2);
    mvvline(rect.top + 1, rect.left + rect.width - 1, ACS_VLINE, rect.height - 2);
    mvaddch(rect.top, rect.left, ACS_ULCORNER);
    mvaddch(rect.top, rect.left + rect.width - 1, ACS_URCORNER);
    mvaddch(rect.top + rect.height - 1, rect.left, ACS_LLCORNER);
    mvaddch(rect.top + rect.height - 1, rect.left + rect.width - 1, ACS_LRCORNER);

    DrawClipped(rect.top + 1, rect.left + 3, rect.width - 6, dialog.title);
    DrawClipped(rect.top + 2, rect.left + 3, rect.width - 6, "Fill arguments manually. CLI arguments remain supported.");

    const int labelWidth = std::min(18, std::max(10, rect.width / 4));
    const int inputColumn = rect.left + 3 + labelWidth;
    const int inputWidth = std::max(10, rect.width - labelWidth - 7);

    for (std::size_t index = 0; index < dialog.fields.size(); ++index) {
        auto& field = dialog.fields[index];
        const int row = DialogFieldRow(rect, index);
        DrawClipped(row, rect.left + 3, labelWidth - 1, field.label + ":");
        if (dialog.focusedControl == index) attron(A_REVERSE);
        const std::string value = field.value.empty() && field.kind == DialogFieldKind::DropList ? "(no options)" : field.value;
        DrawClipped(row, inputColumn, inputWidth, "[ " + value + (field.kind == DialogFieldKind::DropList ? "  v" : "") + " ]");
        if (dialog.focusedControl == index) attroff(A_REVERSE);
    }

    const std::size_t submit = dialog.fields.size();
    const std::size_t cancel = submit + 1;
    const int buttonRow = rect.top + rect.height - 3;
    const int submitColumn = rect.left + rect.width / 2 - 14;
    const int cancelColumn = rect.left + rect.width / 2 + 3;

    if (dialog.focusedControl == submit) attron(A_REVERSE);
    DrawClipped(buttonRow, submitColumn, 12, "[ Submit ]");
    if (dialog.focusedControl == submit) attroff(A_REVERSE);
    if (dialog.focusedControl == cancel) attron(A_REVERSE);
    DrawClipped(buttonRow, cancelColumn, 12, "[ Cancel ]");
    if (dialog.focusedControl == cancel) attroff(A_REVERSE);

    DrawClipped(rect.top + rect.height - 2, rect.left + 3, rect.width - 6, dialog.validationMessage);

    if (dialog.focusedControl < dialog.fields.size() &&
        dialog.fields[dialog.focusedControl].kind == DialogFieldKind::TextInput) {
        curs_set(1);
        move(
            DialogFieldRow(rect, dialog.focusedControl),
            std::min(
                inputColumn + inputWidth - 2,
                inputColumn + 2 + static_cast<int>(dialog.fields[dialog.focusedControl].cursor)
            )
        );
    } else {
        curs_set(0);
    }
}

std::vector<Application::Suggestion> Application::BuildSuggestions() const {
    std::vector<Suggestion> result;
    if (commandBuffer_.empty() || commandBuffer_.find_first_of(" \t") != std::string::npos) return result;

    std::string query = commandBuffer_;
    std::transform(query.begin(), query.end(), query.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });

    for (const auto& command : Commands) {
        std::string name(command.name);
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        if (lower.find(query) != std::string::npos) {
            result.push_back({name + "  -  " + std::string(command.description), name + " "});
        }
        if (result.size() == MaxSuggestions) break;
    }
    return result;
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
    if (!command.empty() && (commandHistory_.empty() || commandHistory_.back() != command)) {
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
}

void Application::ResetHistoryNavigation() {
    historyIndex_.reset();
    commandBeforeHistory_.clear();
}

void Application::EnterManualSelect(const std::optional<std::string>& id) {
    if (tree_.Empty()) {
        status_ = "Cannot enter manualSelect: tree is empty.";
        return;
    }
    if (id) {
        if (!tree_.GetNode(*id)) {
            status_ = "Node ID does not exist: " + *id;
            return;
        }
        selectedId_ = *id;
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
    index = delta < 0
        ? (index == 0 ? visible.size() - 1 : index - 1)
        : (index + 1) % visible.size();
    selectedId_ = visible[index].node->id;
}

void Application::SelectParent() {
    if (const auto parent = tree_.ParentOf(selectedId_)) selectedId_ = *parent;
}

void Application::SelectFirstChild() {
    if (const auto child = tree_.FirstChildOf(selectedId_)) selectedId_ = *child;
}

void Application::EnsureSelection() {
    if (!selectedId_.empty() && tree_.GetNode(selectedId_)) return;
    const auto visible = tree_.Flatten();
    selectedId_ = visible.empty() ? std::string{} : visible.front().node->id;
}

bool Application::OpenCommandDialog(const std::string& command) {
    CommandDialog dialog;
    dialog.command = command;
    dialog.title = "Command: " + command;

    const auto makeText = [](std::string label, std::string value = {}) {
        DialogField field;
        field.label = std::move(label);
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
        std::string parent = "root";
        if (const auto* node = tree_.GetNode(selectedId_)) {
            if (node->kind == NodeKind::Folder) parent = node->id;
            else if (!node->parentId.empty()) parent = node->parentId;
        }
        dialog.fields.push_back(makeDrop("Parent", NodeIdOptions(true, true), parent));
        dialog.fields.push_back(makeText("Name"));
    } else if (command == "setDescription") {
        dialog.fields.push_back(makeDrop("Node", NodeIdOptions(false, false), selectedId_));
        std::string description;
        if (const auto* node = tree_.GetNode(selectedId_)) description = node->description;
        dialog.fields.push_back(makeText("Description", description));
    } else if (command == "remove" || command == "select" || command == "manualSelect") {
        dialog.fields.push_back(makeDrop("Node", NodeIdOptions(false, false), selectedId_));
    } else if (command == "defineColor") {
        dialog.fields.push_back(makeText("Color name"));
        dialog.fields.push_back(makeText("RGB", "rgb(255,255,255)"));
    } else if (command == "color") {
        auto targets = NodeIdOptions(false, false);
        targets.insert(targets.begin(), "default");
        dialog.fields.push_back(makeDrop("Target", std::move(targets), selectedId_.empty() ? "default" : selectedId_));
        dialog.fields.push_back(makeDrop("Foreground", ColorNameOptions(), "white"));
        dialog.fields.push_back(makeDrop("Background", ColorNameOptions(), "black"));
    } else {
        return false;
    }

    commandDialog_ = std::move(dialog);
    status_ = "Argument window opened for: " + command;
    return true;
}

void Application::CloseCommandDialog() {
    commandDialog_.reset();
    curs_set(1);
}

void Application::HandleCommandDialogInput(int key) {
    if (!commandDialog_) return;
    auto& dialog = *commandDialog_;

    if (key == 27) {
        CloseCommandDialog();
        return;
    }
    if (key == KEY_F(2)) {
        SubmitCommandDialog();
        return;
    }
    if (key == '\t') {
        MoveDialogFocus(1);
        return;
    }

    const std::size_t submit = dialog.fields.size();
    const std::size_t cancel = submit + 1;

    if (dialog.focusedControl == submit) {
        if (key == '\n' || key == KEY_ENTER || key == ' ') SubmitCommandDialog();
        return;
    }
    if (dialog.focusedControl == cancel) {
        if (key == '\n' || key == KEY_ENTER || key == ' ') CloseCommandDialog();
        return;
    }
    if (dialog.focusedControl >= dialog.fields.size()) return;

    auto& field = dialog.fields[dialog.focusedControl];
    if (field.kind == DialogFieldKind::DropList) {
        if (field.options.empty()) return;
        if (key == KEY_UP) {
            field.selectedOption = field.selectedOption == 0 ? field.options.size() - 1 : field.selectedOption - 1;
        } else if (key == KEY_DOWN) {
            field.selectedOption = (field.selectedOption + 1) % field.options.size();
        } else if (key == '\n' || key == KEY_ENTER) {
            MoveDialogFocus(1);
            return;
        }
        field.value = field.options[field.selectedOption];
        return;
    }

    if (key == KEY_LEFT) field.cursor = PreviousUtf8Boundary(field.value, field.cursor);
    else if (key == KEY_RIGHT) field.cursor = NextUtf8Boundary(field.value, field.cursor);
    else if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (field.cursor > 0) {
            const auto previous = PreviousUtf8Boundary(field.value, field.cursor);
            field.value.erase(previous, field.cursor - previous);
            field.cursor = previous;
        }
    } else if (key == '\n' || key == KEY_ENTER) {
        MoveDialogFocus(1);
    } else if (key >= 32 && key <= 255) {
        field.value.insert(
            field.value.begin() + static_cast<std::ptrdiff_t>(field.cursor),
            static_cast<char>(key)
        );
        ++field.cursor;
    }
}

void Application::HandleCommandDialogMouse(int mouseX, int mouseY) {
    if (!commandDialog_) return;
    auto& dialog = *commandDialog_;
    const auto rect = CalculateDialogRect(dialog.fields.size());

    const int labelWidth = std::min(18, std::max(10, rect.width / 4));
    const int inputColumn = rect.left + 3 + labelWidth;
    const int inputWidth = std::max(10, rect.width - labelWidth - 7);

    for (std::size_t index = 0; index < dialog.fields.size(); ++index) {
        if (mouseY == DialogFieldRow(rect, index) &&
            mouseX >= inputColumn && mouseX < inputColumn + inputWidth) {
            dialog.focusedControl = index;
            return;
        }
    }

    const int buttonRow = rect.top + rect.height - 3;
    const int submitColumn = rect.left + rect.width / 2 - 14;
    const int cancelColumn = rect.left + rect.width / 2 + 3;
    if (mouseY == buttonRow && mouseX >= submitColumn && mouseX < submitColumn + 12) {
        SubmitCommandDialog();
    } else if (mouseY == buttonRow && mouseX >= cancelColumn && mouseX < cancelColumn + 12) {
        CloseCommandDialog();
    }
}

void Application::MoveDialogFocus(int delta) {
    if (!commandDialog_) return;
    auto& dialog = *commandDialog_;
    const std::size_t count = dialog.fields.size() + 2;
    dialog.focusedControl = delta < 0
        ? (dialog.focusedControl == 0 ? count - 1 : dialog.focusedControl - 1)
        : (dialog.focusedControl + 1) % count;
}

bool Application::SubmitCommandDialog() {
    if (!commandDialog_) return false;
    for (const auto& field : commandDialog_->fields) {
        if (field.value.empty()) {
            commandDialog_->validationMessage = "Required field is empty: " + field.label;
            return false;
        }
    }

    const std::string line = BuildDialogCommand();
    if (line.empty()) {
        commandDialog_->validationMessage = "Could not build command.";
        return false;
    }

    CloseCommandDialog();
    AddCommandToHistory(line);
    ExecuteCommand(line);
    return true;
}

std::string Application::BuildDialogCommand() const {
    if (!commandDialog_) return {};
    const auto& dialog = *commandDialog_;

    if (dialog.command == "color" && dialog.fields.size() == 3) {
        const std::string& target = dialog.fields[0].value;
        const std::string& foreground = dialog.fields[1].value;
        const std::string& background = dialog.fields[2].value;
        if (target == "default") {
            return "color " + QuoteArgument(foreground) + " bg " + QuoteArgument(background);
        }
        return "color " + QuoteArgument(target) + " " + QuoteArgument(foreground) + " bg " + QuoteArgument(background);
    }

    std::string result = dialog.command;
    for (const auto& field : dialog.fields) result += " " + QuoteArgument(field.value);
    return result;
}

std::vector<std::string> Application::NodeIdOptions(bool foldersOnly, bool includeRoot) const {
    std::vector<std::string> options;
    if (includeRoot) options.push_back("root");
    for (const auto& visible : tree_.Flatten()) {
        if (visible.node && (!foldersOnly || visible.node->kind == NodeKind::Folder)) {
            options.push_back(visible.node->id);
        }
    }
    return options;
}

std::vector<std::string> Application::ColorNameOptions() const {
    std::vector<std::string> options{
        "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white",
        "brightBlack", "brightRed", "brightGreen", "brightYellow", "brightBlue",
        "brightMagenta", "brightCyan", "brightWhite"
    };
    for (const auto& [name, color] : definedColors_) {
        (void)color;
        options.push_back(name);
    }
    std::sort(options.begin(), options.end());
    options.erase(std::unique(options.begin(), options.end()), options.end());
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
    std::string result = "\"";
    for (const char character : value) {
        if (character == '\\' || character == '\"') result += '\\';
        result += character;
    }
    result += '\"';
    return result;
}

std::vector<std::string> Application::Tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool quoted = false;
    bool escaping = false;

    for (const char character : line) {
        if (escaping) {
            current += character;
            escaping = false;
        } else if (character == '\\') {
            escaping = true;
        } else if (character == '\"') {
            quoted = !quoted;
        } else if (!quoted && std::isspace(static_cast<unsigned char>(character))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += character;
        }
    }
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
