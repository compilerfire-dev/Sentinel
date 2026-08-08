#include "CommandProcessor.hpp"

#include <charconv>
#include <string_view>

CommandProcessor::CommandProcessor(TaskManager& manager)
    : manager_(manager) {}

std::optional<std::size_t> CommandProcessor::ParseIndex(const std::string& value) {
    if (value.empty()) {
        status_ = "Expected a task index.";
        return std::nullopt;
    }

    std::size_t index = 0;
    const std::string_view view(value);
    const auto [pointer, error] = std::from_chars(view.data(), view.data() + view.size(), index);
    if (error != std::errc{} || pointer != view.data() + view.size()) {
        status_ = "Task index must be a non-negative integer.";
        return std::nullopt;
    }
    return index;
}

void CommandProcessor::Execute(const std::string& line) {
    const auto separator = line.find_first_of(" \t");
    const std::string command = line.substr(0, separator);
    std::string argument = separator == std::string::npos ? std::string{} : line.substr(separator + 1);
    const auto first = argument.find_first_not_of(" \t");
    argument = first == std::string::npos ? std::string{} : argument.substr(first);
    searchResults_.reset();

    if (command.empty()) { status_.clear(); return; }
    if (command == "quit" || command == "exit") { quit_ = true; return; }
    if (command == "help") { status_ = "Commands: add, remove, start, stop, done, search, list, help, quit"; return; }
    if (command == "list") { status_ = "Showing all tasks."; return; }

    if (command == "add") {
        if (argument.empty()) { status_ = "Usage: add <task name>"; return; }
        manager_.AddTask(argument);
        status_ = "Task added.";
        return;
    }

    if (command == "search") {
        if (argument.empty()) { status_ = "Usage: search <text>"; return; }
        searchResults_ = manager_.Search(argument);
        status_ = "Search results: " + std::to_string(searchResults_->size());
        return;
    }

    const auto index = ParseIndex(argument);
    if (!index) return;

    if (command == "remove") {
        status_ = manager_.RemoveTask(*index) ? "Task removed." : "Task index does not exist.";
        return;
    }

    Task* task = manager_.GetTask(*index);
    if (!task) { status_ = "Task index does not exist."; return; }

    if (command == "start") {
        if (task->IsCompleted()) { status_ = "Completed tasks cannot be restarted."; return; }
        task->Start();
        status_ = "Task started.";
    } else if (command == "stop") {
        task->Stop();
        status_ = "Task stopped.";
    } else if (command == "done") {
        task->Complete();
        status_ = "Task completed.";
    } else {
        status_ = "Unknown command: " + command;
    }
}

bool CommandProcessor::ShouldQuit() const noexcept { return quit_; }
const std::string& CommandProcessor::GetStatusMessage() const noexcept { return status_; }
const std::optional<std::vector<std::size_t>>& CommandProcessor::GetSearchResults() const noexcept { return searchResults_; }
