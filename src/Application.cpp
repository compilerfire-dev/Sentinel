#include "Application.hpp"
#include "FuzzySearch.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <ncurses.h>
#include <sstream>
#include <string_view>

namespace {

constexpr std::size_t MaxSuggestions = 6;

struct CommandDefinition {
    std::string_view name;
    std::string_view description;
};

constexpr std::array<CommandDefinition, 9> Commands{{
    {"add", "add a new task"},
    {"remove", "remove a task"},
    {"start", "start or resume a task"},
    {"stop", "stop a running task"},
    {"done", "complete a task"},
    {"search", "fuzzy-search tasks"},
    {"list", "show all tasks"},
    {"help", "show command help"},
    {"quit", "exit Sentinel"},
}};

bool TakesTaskArgument(std::string_view command) {
    return command == "remove" || command == "start" || command == "stop" ||
           command == "done" || command == "search";
}

std::string TrimLeft(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    return first == std::string::npos ? std::string{} : value.substr(first);
}

} // namespace

Application::Application()
    : commandProcessor_(taskManager_) {}

int Application::Run() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    timeout(100);
    mousemask(ALL_MOUSE_EVENTS, nullptr);

    while (!commandProcessor_.ShouldQuit()) {
        HandleInput();
        Render();
    }

    endwin();
    return 0;
}

void Application::HandleInput() {
    const int key = getch();
    if (key == ERR) return;

    const auto suggestions = BuildSuggestions();

    if (key == KEY_MOUSE) {
        HandleMouse();
        return;
    }

    if (key == '\t') {
        AcceptSuggestion(suggestions);
        return;
    }

    if (key == KEY_UP || key == KEY_DOWN) {
        if (suggestions.empty()) return;

        if (!navigatingSuggestions_) {
            navigatingSuggestions_ = true;
            selectedSuggestion_ = key == KEY_UP ? suggestions.size() - 1 : 0;
            return;
        }

        if (key == KEY_UP) {
            selectedSuggestion_ = selectedSuggestion_ == 0
                ? suggestions.size() - 1
                : selectedSuggestion_ - 1;
        } else {
            selectedSuggestion_ = (selectedSuggestion_ + 1) % suggestions.size();
        }
        return;
    }

    if (key == '\n' || key == KEY_ENTER) {
        if (navigatingSuggestions_ && !suggestions.empty()) {
            AcceptSuggestion(suggestions);
            return;
        }

        commandProcessor_.Execute(commandBuffer_);
        commandBuffer_.clear();
        ResetSuggestionSelection();
        return;
    }

    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (!commandBuffer_.empty()) commandBuffer_.pop_back();
        ResetSuggestionSelection();
        return;
    }

    if (key >= 32 && key <= 126) {
        commandBuffer_.push_back(static_cast<char>(key));
        ResetSuggestionSelection();
    }
}

void Application::HandleMouse() {
    MEVENT event{};
    if (getmouse(&event) != OK) return;
    if ((event.bstate & BUTTON1_CLICKED) == 0 &&
        (event.bstate & BUTTON1_PRESSED) == 0) return;

    const auto suggestions = BuildSuggestions();
    if (suggestions.empty()) return;

    const int startRow = SuggestionStartRow(suggestions.size());
    if (event.y < startRow || event.y >= startRow + static_cast<int>(suggestions.size())) {
        return;
    }

    selectedSuggestion_ = static_cast<std::size_t>(event.y - startRow);
    navigatingSuggestions_ = true;
    AcceptSuggestion(suggestions);
}

void Application::AcceptSuggestion(const std::vector<Suggestion>& suggestions) {
    if (suggestions.empty()) return;

    selectedSuggestion_ = std::min(selectedSuggestion_, suggestions.size() - 1);
    commandBuffer_ = suggestions[selectedSuggestion_].replacement;
    ResetSuggestionSelection();
}

void Application::ResetSuggestionSelection() {
    selectedSuggestion_ = 0;
    navigatingSuggestions_ = false;
}

void Application::Render() {
    erase();
    const auto suggestions = BuildSuggestions();
    if (!suggestions.empty() && selectedSuggestion_ >= suggestions.size()) {
        selectedSuggestion_ = 0;
    }

    RenderHeader();
    RenderTasks();
    RenderSuggestions(suggestions);
    RenderStatus();
    RenderCommandLine();
    refresh();
}

void Application::RenderHeader() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)height;
    mvaddnstr(0, 0, "Sentinel - Productivity Tracker", std::max(0, width - 1));
    if (width > 1) mvhline(1, 0, ACS_HLINE, width - 1);
}

std::vector<Application::Suggestion> Application::BuildSuggestions() const {
    std::vector<Suggestion> suggestions;
    if (commandBuffer_.empty()) return suggestions;

    const auto separator = commandBuffer_.find_first_of(" \t");
    const std::string command = commandBuffer_.substr(0, separator);

    if (separator == std::string::npos) {
        struct RankedCommand {
            int score;
            CommandDefinition definition;
        };

        std::vector<RankedCommand> ranked;
        for (const auto& definition : Commands) {
            const auto score = FuzzySearch::Score(command, definition.name);
            if (score) ranked.push_back({*score, definition});
        }

        std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
            if (left.score != right.score) return left.score > right.score;
            return left.definition.name < right.definition.name;
        });

        for (std::size_t index = 0; index < ranked.size() && index < MaxSuggestions; ++index) {
            const auto& item = ranked[index];
            suggestions.push_back({
                std::string(item.definition.name) + "  -  " + std::string(item.definition.description),
                std::string(item.definition.name) + " "
            });
        }
        return suggestions;
    }

    if (!TakesTaskArgument(command)) return suggestions;

    const std::string query = TrimLeft(commandBuffer_.substr(separator + 1));
    std::vector<std::size_t> taskIndices;

    if (query.empty()) {
        taskIndices.reserve(taskManager_.GetTasks().size());
        for (std::size_t index = 0; index < taskManager_.GetTasks().size(); ++index) {
            taskIndices.push_back(index);
        }
    } else {
        taskIndices = taskManager_.Search(query);
    }

    for (std::size_t position = 0;
         position < taskIndices.size() && suggestions.size() < MaxSuggestions;
         ++position) {
        const std::size_t taskIndex = taskIndices[position];
        const Task* task = taskManager_.GetTask(taskIndex);
        if (!task) continue;

        std::ostringstream label;
        label << taskIndex << "  " << task->GetName();
        if (task->IsCompleted()) label << "  [completed]";
        else if (task->IsRunning()) label << "  [running]";

        suggestions.push_back({
            label.str(),
            command + " " + task->GetName()
        });
    }

    return suggestions;
}

std::vector<std::size_t> Application::VisibleTaskIndices() const {
    if (commandProcessor_.GetSearchResults()) {
        return *commandProcessor_.GetSearchResults();
    }

    std::vector<std::size_t> indices;
    indices.reserve(taskManager_.GetTasks().size());
    for (std::size_t index = 0; index < taskManager_.GetTasks().size(); ++index) {
        indices.push_back(index);
    }
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

    for (const std::size_t index : VisibleTaskIndices()) {
        if (row >= lastTaskRow) break;

        const Task* task = taskManager_.GetTask(index);
        if (!task) continue;

        const char* state = task->IsCompleted() ? "[x]" : (task->IsRunning() ? "[>]" : "[ ]");
        std::ostringstream line;
        line << std::setw(3) << index << ' ' << state << ' ' << task->GetName()
             << " - " << FormatDuration(task->GetElapsedTime())
             << " - " << task->GetCompletionDateString();
        mvaddnstr(row++, 0, line.str().c_str(), std::max(0, width - 1));
    }
}

void Application::RenderSuggestions(const std::vector<Suggestion>& suggestions) {
    if (suggestions.empty()) return;

    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)height;

    const int startRow = SuggestionStartRow(suggestions.size());
    for (std::size_t index = 0; index < suggestions.size(); ++index) {
        const bool selected = index == selectedSuggestion_;
        if (selected) attron(A_REVERSE);

        const std::string line = (selected ? "> " : "  ") + suggestions[index].label;
        mvaddnstr(
            startRow + static_cast<int>(index),
            0,
            line.c_str(),
            std::max(0, width - 1)
        );

        if (selected) attroff(A_REVERSE);
    }
}

void Application::RenderStatus() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    if (height < 2) return;

    std::string status = commandProcessor_.GetStatusMessage();
    if (!BuildSuggestions().empty()) {
        status += status.empty() ? "" : " | ";
        status += "Tab complete | Up/Down select | click select";
    }

    mvaddnstr(height - 2, 0, status.c_str(), std::max(0, width - 1));
}

void Application::RenderCommandLine() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    if (height < 1 || width < 1) return;

    const int row = height - 1;
    mvaddnstr(row, 0, "> ", std::max(0, width - 1));
    if (width > 2) mvaddnstr(row, 2, commandBuffer_.c_str(), width - 3);
    move(row, std::min(width - 1, static_cast<int>(commandBuffer_.size()) + 2));
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
