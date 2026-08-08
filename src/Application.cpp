#include "Application.hpp"

#include <algorithm>
#include <iomanip>
#include <ncurses.h>
#include <sstream>

Application::Application()
    : commandProcessor_(taskManager_) {}

int Application::Run() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    timeout(100);

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

    if (key == '\n' || key == KEY_ENTER) {
        commandProcessor_.Execute(commandBuffer_);
        commandBuffer_.clear();
        return;
    }

    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (!commandBuffer_.empty()) commandBuffer_.pop_back();
        return;
    }

    if (key >= 32 && key <= 126) {
        commandBuffer_.push_back(static_cast<char>(key));
    }
}

void Application::Render() {
    erase();
    RenderHeader();
    RenderTasks();
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

void Application::RenderTasks() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);

    int row = 2;
    const int lastTaskRow = std::max(2, height - 3);
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

void Application::RenderStatus() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    if (height >= 2) {
        mvaddnstr(height - 2, 0, commandProcessor_.GetStatusMessage().c_str(), std::max(0, width - 1));
    }
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
