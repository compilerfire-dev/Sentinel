#include "CommandProcessor.hpp"

#include <charconv>
#include <regex>
#include <string_view>

namespace {

const std::vector<std::string> CommandLines{
    "add <task name>                 Add a new task",
    "remove <id | fuzzy name>        Remove a task",
    "start <id | fuzzy name>         Start or resume a task",
    "stop <id | fuzzy name>          Stop a running task",
    "done <id | fuzzy name>          Complete a task",
    "search <fuzzy text>             Show fuzzy-matched tasks",
    "list                            Show all tasks",
    "commands                        Show all commands",
    "setJsonFile <path.json>         Switch active JSON data file",
    "color rgb(r,g,b) bg rgb(r,g,b) Set foreground/background colors",
    "help                            Show short help",
    "quit                            Save and exit Sentinel"
};

} // namespace

CommandProcessor::CommandProcessor(TaskManager& manager, DisplaySettings& displaySettings)
    : manager_(manager), displaySettings_(displaySettings) {}

std::optional<std::size_t> CommandProcessor::ParseNumericIndex(const std::string& value) const {
    if (value.empty()) return std::nullopt;
    std::size_t index = 0;
    const std::string_view view(value);
    const auto [pointer, error] = std::from_chars(view.data(), view.data() + view.size(), index);
    if (error != std::errc{} || pointer != view.data() + view.size()) return std::nullopt;
    return index;
}

std::optional<std::size_t> CommandProcessor::ResolveTaskReference(const std::string& value) {
    if (value.empty()) {
        status_ = "Expected a task index or fuzzy task name.";
        return std::nullopt;
    }

    if (const auto numericIndex = ParseNumericIndex(value)) {
        if (manager_.GetTask(*numericIndex)) return numericIndex;
        status_ = "Task index does not exist.";
        return std::nullopt;
    }

    const auto fuzzyIndex = manager_.FindBestMatch(value);
    if (!fuzzyIndex) {
        status_ = "No task matches: " + value;
        return std::nullopt;
    }
    return fuzzyIndex;
}

void CommandProcessor::Autosave() {
    std::string error;
    if (!manager_.Save(error)) status_ += " | Autosave failed: " + error;
}

bool CommandProcessor::ParseColorCommand(const std::string& argument) {
    static const std::regex pattern(
        R"(^\s*(?:rgb|rpg)\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*\)\s+bg\s+(?:rgb|rpg)\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*\)\s*$)",
        std::regex::icase
    );

    std::smatch match;
    if (!std::regex_match(argument, match, pattern)) {
        status_ = "Usage: color rgb(r,g,b) bg rgb(r,g,b)";
        return false;
    }

    int values[6]{};
    for (int index = 0; index < 6; ++index) {
        values[index] = std::stoi(match[index + 1].str());
        if (values[index] < 0 || values[index] > 255) {
            status_ = "RGB channels must be between 0 and 255.";
            return false;
        }
    }

    displaySettings_.foreground = {values[0], values[1], values[2]};
    displaySettings_.background = {values[3], values[4], values[5]};
    displaySettings_.dirty = true;
    status_ = "Display colors updated.";
    return true;
}

void CommandProcessor::Execute(const std::string& line) {
    const auto separator = line.find_first_of(" \t");
    const std::string command = line.substr(0, separator);
    std::string argument = separator == std::string::npos ? std::string{} : line.substr(separator + 1);
    const auto first = argument.find_first_not_of(" \t");
    argument = first == std::string::npos ? std::string{} : argument.substr(first);

    searchResults_.reset();
    infoLines_.clear();

    if (command.empty()) { status_.clear(); return; }

    if (command == "quit" || command == "exit") {
        std::string error;
        manager_.Save(error);
        if (!error.empty()) status_ = "Save failed: " + error;
        quit_ = true;
        return;
    }

    if (command == "commands") {
        infoLines_ = CommandLines;
        status_ = "Available Sentinel commands.";
        return;
    }

    if (command == "help") {
        status_ = "Type 'commands' to show every command and its syntax.";
        return;
    }

    if (command == "list") {
        status_ = "Showing all tasks.";
        return;
    }

    if (command == "setJsonFile") {
        if (argument.empty()) { status_ = "Usage: setJsonFile <json_file_path.json>"; return; }
        std::string error;
        if (manager_.SetJsonFile(argument, error)) {
            status_ = "JSON data file: " + manager_.GetJsonFile();
        } else {
            status_ = "Could not switch JSON file: " + error;
        }
        return;
    }

    if (command == "color") {
        ParseColorCommand(argument);
        return;
    }

    if (command == "add") {
        if (argument.empty()) { status_ = "Usage: add <task name>"; return; }
        manager_.AddTask(argument);
        status_ = "Task added.";
        Autosave();
        return;
    }

    if (command == "search") {
        if (argument.empty()) { status_ = "Usage: search <fuzzy text>"; return; }
        searchResults_ = manager_.Search(argument);
        status_ = "Fuzzy search results: " + std::to_string(searchResults_->size());
        return;
    }

    if (command != "remove" && command != "start" && command != "stop" && command != "done") {
        status_ = "Unknown command: " + command;
        return;
    }

    const auto index = ResolveTaskReference(argument);
    if (!index) return;

    if (command == "remove") {
        status_ = manager_.RemoveTask(*index) ? "Task removed." : "Task does not exist.";
        Autosave();
        return;
    }

    Task* task = manager_.GetTask(*index);
    if (!task) { status_ = "Task does not exist."; return; }

    if (command == "start") {
        if (task->IsCompleted()) { status_ = "Completed tasks cannot be restarted."; return; }
        task->Start();
        status_ = "Task started: " + task->GetName();
    } else if (command == "stop") {
        task->Stop();
        status_ = "Task stopped: " + task->GetName();
    } else {
        task->Complete();
        status_ = "Task completed: " + task->GetName();
    }
    Autosave();
}

bool CommandProcessor::ShouldQuit() const noexcept { return quit_; }
const std::string& CommandProcessor::GetStatusMessage() const noexcept { return status_; }
const std::optional<std::vector<std::size_t>>& CommandProcessor::GetSearchResults() const noexcept { return searchResults_; }
const std::vector<std::string>& CommandProcessor::GetInfoLines() const noexcept { return infoLines_; }
