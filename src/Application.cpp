#include "Application.hpp"
#include "FuzzySearch.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <ncurses.h>
#include <sstream>
#include <string_view>

namespace {

constexpr std::size_t MaxSuggestions = 6;
constexpr short TaskColorPair = 1;
constexpr short FirstPerTaskPair = 2;

struct CommandDefinition {
    std::string_view name;
    std::string_view description;
};

constexpr std::array<CommandDefinition, 13> Commands{{
    {"add", "add a new task with an ID"},
    {"remove", "remove a task"},
    {"start", "start or resume a task"},
    {"stop", "stop a running task"},
    {"done", "complete a task"},
    {"search", "fuzzy-search tasks"},
    {"list", "show all tasks"},
    {"commands", "show all commands"},
    {"setJsonFile", "switch active JSON data file"},
    {"defineColor", "define a named RGB color"},
    {"color", "set default or per-task color"},
    {"help", "show command help"},
    {"quit", "save and exit Sentinel"},
}};

bool TakesTaskArgument(std::string_view command) {
    return command == "remove" || command == "start" || command == "stop" ||
           command == "done" || command == "search";
}

std::string TrimLeft(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    return first == std::string::npos ? std::string{} : value.substr(first);
}

std::string UnquotePartial(std::string value) {
    value = TrimLeft(std::move(value));
    if (!value.empty() && value.front() == '"') value.erase(value.begin());
    if (!value.empty() && value.back() == '"') value.pop_back();
    return value;
}

std::string QuoteIfNeeded(const std::string& value) {
    if (value.find_first_of(" \t") == std::string::npos) return value;
    return "\"" + value + "\"";
}

short NearestBasicColor(const RgbColor& color) {
    struct BasicColor {
        short ncursesColor;
        int red;
        int green;
        int blue;
    };

    constexpr std::array<BasicColor, 8> basic{{
        {COLOR_BLACK, 0, 0, 0},
        {COLOR_RED, 255, 0, 0},
        {COLOR_GREEN, 0, 255, 0},
        {COLOR_YELLOW, 255, 255, 0},
        {COLOR_BLUE, 0, 0, 255},
        {COLOR_MAGENTA, 255, 0, 255},
        {COLOR_CYAN, 0, 255, 255},
        {COLOR_WHITE, 255, 255, 255},
    }};

    long bestDistance = std::numeric_limits<long>::max();
    short best = COLOR_WHITE;

    for (const auto& candidate : basic) {
        const long dr = color.red - candidate.red;
        const long dg = color.green - candidate.green;
        const long db = color.blue - candidate.blue;
        const long distance = dr * dr + dg * dg + db * db;

        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate.ncursesColor;
        }
    }

    return best;
}

short NearestTerminalColor(const RgbColor& color) {
    if (!has_colors() || COLORS <= 0) {
        return NearestBasicColor(color);
    }

    // Do not redefine the terminal's palette with init_color(). Instead,
    // inspect the palette ncurses already exposes and select the closest
    // foreground/background entry independently. This is considerably more
    // portable across xterm-compatible and fixed-palette terminals.
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

        if (color_content(
                static_cast<short>(index),
                &red,
                &green,
                &blue
            ) == ERR) {
            continue;
        }

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

bool InitializeTaskColorPair(
    short pair,
    const RgbColor& foreground,
    const RgbColor& background
) {
    if (!has_colors() || pair <= 0 || pair >= COLOR_PAIRS) {
        return false;
    }

    const short foregroundIndex = NearestTerminalColor(foreground);
    const short backgroundIndex = NearestTerminalColor(background);

    return init_pair(pair, foregroundIndex, backgroundIndex) != ERR;
}

} // namespace

Application::Application()
    : commandProcessor_(taskManager_, displaySettings_),
      lastAutosave_(std::chrono::steady_clock::now()) {
    std::string error;
    if (!taskManager_.Load(error)) persistenceStatus_ = "Load failed: " + error;
}

int Application::Run() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    timeout(100);
    mousemask(ALL_MOUSE_EVENTS, nullptr);

    if (has_colors()) start_color();
    ApplyColors();

    while (!commandProcessor_.ShouldQuit()) {
        HandleInput();
        PeriodicAutosave();
        if (displaySettings_.dirty) ApplyColors();
        Render();
    }

    std::string error;
    taskManager_.Save(error);
    endwin();
    return 0;
}

void Application::PeriodicAutosave() {
    const auto now = std::chrono::steady_clock::now();
    if (now - lastAutosave_ < std::chrono::seconds(1)) return;
    lastAutosave_ = now;
    std::string error;
    if (!taskManager_.Save(error)) persistenceStatus_ = "Autosave failed: " + error;
}

void Application::ApplyColors() {
    displaySettings_.dirty = false;

    if (!has_colors()) {
        persistenceStatus_ = "Terminal does not support colors.";
        return;
    }

    if (!InitializeTaskColorPair(
            TaskColorPair,
            displaySettings_.foreground,
            displaySettings_.background
        )) {
        persistenceStatus_ = "Terminal could not initialize the default task color pair.";
    }

    // Pair zero remains the terminal's normal UI style. Only task rows select
    // TaskColorPair or a per-task pair while they are being rendered.
    bkgd(A_NORMAL);
    attr_set(A_NORMAL, 0, nullptr);
}

void Application::AddCommandToHistory(const std::string& command) {
    if (command.empty()) return;
    if (commandHistory_.empty() || commandHistory_.back() != command) commandHistory_.push_back(command);
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
    ResetSuggestionSelection();
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
    ResetSuggestionSelection();
}

void Application::ResetHistoryNavigation() {
    historyIndex_.reset();
    commandBeforeHistory_.clear();
}

void Application::HandleInput() {
    const int key = getch();
    if (key == ERR) return;
    const auto suggestions = BuildSuggestions();

    if (key == KEY_MOUSE) { HandleMouse(); return; }
    if (key == '\t') { AcceptSuggestion(suggestions); ResetHistoryNavigation(); return; }

    if (key == KEY_LEFT) {
        if (cursorPosition_ > 0) --cursorPosition_;
        ResetHistoryNavigation();
        ResetSuggestionSelection();
        return;
    }

    if (key == KEY_RIGHT) {
        if (cursorPosition_ < commandBuffer_.size()) ++cursorPosition_;
        ResetHistoryNavigation();
        ResetSuggestionSelection();
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
        if (navigatingSuggestions_ && !suggestions.empty()) { AcceptSuggestion(suggestions); return; }
        const std::string executedCommand = commandBuffer_;
        commandProcessor_.Execute(executedCommand);
        AddCommandToHistory(executedCommand);
        commandBuffer_.clear();
        cursorPosition_ = 0;
        persistenceStatus_.clear();
        ResetSuggestionSelection();
        return;
    }

    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (cursorPosition_ > 0 && !commandBuffer_.empty()) {
            commandBuffer_.erase(cursorPosition_ - 1, 1);
            --cursorPosition_;
        }
        ResetHistoryNavigation();
        ResetSuggestionSelection();
        return;
    }

    if (key >= 32 && key <= 126) {
        commandBuffer_.insert(commandBuffer_.begin() + static_cast<std::ptrdiff_t>(cursorPosition_), static_cast<char>(key));
        ++cursorPosition_;
        ResetHistoryNavigation();
        ResetSuggestionSelection();
    }
}

void Application::HandleMouse() {
    MEVENT event{};
    if (getmouse(&event) != OK) return;
    if ((event.bstate & BUTTON1_CLICKED) == 0 && (event.bstate & BUTTON1_PRESSED) == 0) return;

    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    const int commandRow = height - 1;

    if (event.y == commandRow) {
        const int textColumn = std::max(0, event.x - 2);
        cursorPosition_ = std::min(commandBuffer_.size(), static_cast<std::size_t>(textColumn));
        ResetHistoryNavigation();
        ResetSuggestionSelection();
        return;
    }

    const auto suggestions = BuildSuggestions();
    if (suggestions.empty()) return;
    const int startRow = SuggestionStartRow(suggestions.size());
    if (event.y < startRow || event.y >= startRow + static_cast<int>(suggestions.size())) return;
    selectedSuggestion_ = static_cast<std::size_t>(event.y - startRow);
    navigatingSuggestions_ = true;
    AcceptSuggestion(suggestions);
    ResetHistoryNavigation();
}

void Application::AcceptSuggestion(const std::vector<Suggestion>& suggestions) {
    if (suggestions.empty()) return;
    selectedSuggestion_ = std::min(selectedSuggestion_, suggestions.size() - 1);
    commandBuffer_ = suggestions[selectedSuggestion_].replacement;
    cursorPosition_ = commandBuffer_.size();
    ResetSuggestionSelection();
}

void Application::ResetSuggestionSelection() {
    selectedSuggestion_ = 0;
    navigatingSuggestions_ = false;
}

void Application::Render() {
    erase();
    attr_set(A_NORMAL, 0, nullptr);
    const auto suggestions = BuildSuggestions();
    if (!suggestions.empty() && selectedSuggestion_ >= suggestions.size()) selectedSuggestion_ = 0;
    RenderHeader();
    RenderTasks();
    RenderSuggestions(suggestions);
    RenderStatus();
    RenderCommandLine();
    refresh();
}

void Application::RenderHeader() {
    attr_set(A_NORMAL, 0, nullptr);
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)height;
    const std::string title = "Sentinel - Productivity Tracker | " + taskManager_.GetJsonFile();
    mvaddnstr(0, 0, title.c_str(), std::max(0, width - 1));
    if (width > 1) mvhline(1, 0, ACS_HLINE, width - 1);
}

std::vector<Application::Suggestion> Application::BuildSuggestions() const {
    std::vector<Suggestion> suggestions;
    if (commandBuffer_.empty()) return suggestions;

    const auto separator = commandBuffer_.find_first_of(" \t");
    const std::string command = commandBuffer_.substr(0, separator);

    if (separator == std::string::npos) {
        struct RankedCommand { int score; CommandDefinition definition; };
        std::vector<RankedCommand> ranked;
        for (const auto& definition : Commands) {
            const auto score = FuzzySearch::Score(command, definition.name);
            if (score) ranked.push_back({*score, definition});
        }
        std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
            return left.score != right.score ? left.score > right.score : left.definition.name < right.definition.name;
        });
        for (std::size_t index = 0; index < ranked.size() && index < MaxSuggestions; ++index) {
            suggestions.push_back({
                std::string(ranked[index].definition.name) + "  -  " + std::string(ranked[index].definition.description),
                std::string(ranked[index].definition.name) + " "
            });
        }
        return suggestions;
    }

    if (!TakesTaskArgument(command)) return suggestions;
    const std::string query = UnquotePartial(commandBuffer_.substr(separator + 1));
    std::vector<std::size_t> taskIndices;

    if (query.empty()) {
        for (std::size_t index = 0; index < taskManager_.GetTasks().size(); ++index) taskIndices.push_back(index);
    } else {
        taskIndices = taskManager_.Search(query);
    }

    for (const std::size_t taskIndex : taskIndices) {
        if (suggestions.size() >= MaxSuggestions) break;
        const Task* task = taskManager_.GetTask(taskIndex);
        if (!task) continue;

        std::ostringstream label;
        label << task->GetId() << "  -  " << task->GetName();
        if (task->IsCompleted()) label << "  [completed]";
        else if (task->IsRunning()) label << "  [running]";

        suggestions.push_back({
            label.str(),
            command + " " + QuoteIfNeeded(task->GetId())
        });
    }
    return suggestions;
}

std::vector<std::size_t> Application::VisibleTaskIndices() const {
    if (commandProcessor_.GetSearchResults()) return *commandProcessor_.GetSearchResults();
    std::vector<std::size_t> indices;
    for (std::size_t index = 0; index < taskManager_.GetTasks().size(); ++index) indices.push_back(index);
    return indices;
}

int Application::SuggestionStartRow(std::size_t suggestionCount) const {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)width;
    return std::max(2, height - 2 - static_cast<int>(suggestionCount));
}

void Application::RenderTasks() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    const auto suggestions = BuildSuggestions();
    const int lastTaskRow = SuggestionStartRow(suggestions.size());
    int row = 2;

    if (!commandProcessor_.GetInfoLines().empty()) {
        attr_set(A_NORMAL, 0, nullptr);
        for (const auto& line : commandProcessor_.GetInfoLines()) {
            if (row >= lastTaskRow) break;
            mvaddnstr(row++, 0, line.c_str(), std::max(0, width - 1));
        }
        return;
    }

    std::size_t visibleSlot = 0;
    for (const std::size_t index : VisibleTaskIndices()) {
        if (row >= lastTaskRow) break;
        const Task* task = taskManager_.GetTask(index);
        if (!task) continue;

        const char* state = task->IsCompleted() ? "[x]" : (task->IsRunning() ? "[>]" : "[ ]");
        std::ostringstream line;
        line << task->GetId()
             << ' ' << state
             << ' ' << task->GetName()
             << " - " << FormatDuration(task->GetElapsedTime())
             << " - " << task->GetCompletionDateString();

        short pair = TaskColorPair;

        if (has_colors() && task->HasCustomColor()) {
            const short candidatePair = static_cast<short>(FirstPerTaskPair + visibleSlot);

            if (candidatePair > 0 && candidatePair < COLOR_PAIRS &&
                InitializeTaskColorPair(
                    candidatePair,
                    *task->GetForegroundColor(),
                    *task->GetBackgroundColor()
                )) {
                pair = candidatePair;
            }
        }

        if (has_colors()) {
            // Pass the pair explicitly rather than OR-ing COLOR_PAIR into the
            // attribute mask. This guarantees ncurses receives foreground and
            // background from the same initialized pair.
            attr_set(A_NORMAL, pair, nullptr);
        }

        mvaddnstr(row++, 0, line.str().c_str(), std::max(0, width - 1));
        attr_set(A_NORMAL, 0, nullptr);
        ++visibleSlot;
    }

    attr_set(A_NORMAL, 0, nullptr);
}

void Application::RenderSuggestions(const std::vector<Suggestion>& suggestions) {
    if (suggestions.empty()) return;
    attr_set(A_NORMAL, 0, nullptr);
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)height;
    const int startRow = SuggestionStartRow(suggestions.size());

    for (std::size_t index = 0; index < suggestions.size(); ++index) {
        const bool selected = index == selectedSuggestion_;
        if (selected) attron(A_REVERSE);
        const std::string line = (selected ? "> " : "  ") + suggestions[index].label;
        mvaddnstr(startRow + static_cast<int>(index), 0, line.c_str(), std::max(0, width - 1));
        if (selected) attroff(A_REVERSE);
    }
}

void Application::RenderStatus() {
    attr_set(A_NORMAL, 0, nullptr);
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    if (height < 2) return;

    std::string status = commandProcessor_.GetStatusMessage();
    if (!persistenceStatus_.empty()) {
        status += status.empty() ? "" : " | ";
        status += persistenceStatus_;
    }
    if (!BuildSuggestions().empty()) {
        status += status.empty() ? "" : " | ";
        status += "Tab complete | Down suggestions | Up history | Left/Right cursor | click select";
    } else if (!commandHistory_.empty()) {
        status += status.empty() ? "" : " | ";
        status += "Up: previous command | Left/Right: cursor";
    }
    mvaddnstr(height - 2, 0, status.c_str(), std::max(0, width - 1));
}

void Application::RenderCommandLine() {
    attr_set(A_NORMAL, 0, nullptr);
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    if (height < 1 || width < 1) return;
    const int row = height - 1;
    mvaddnstr(row, 0, "> ", std::max(0, width - 1));
    if (width > 2) mvaddnstr(row, 2, commandBuffer_.c_str(), width - 3);
    move(row, std::min(width - 1, static_cast<int>(cursorPosition_) + 2));
}

std::string Application::FormatDuration(std::chrono::seconds duration) {
    const auto total = duration.count();
    std::ostringstream out;
    out << std::setfill('0')
        << std::setw(2) << total / 3600 << ':'
        << std::setw(2) << (total % 3600) / 60 << ':'
        << std::setw(2) << total % 60;
    return out.str();
}
