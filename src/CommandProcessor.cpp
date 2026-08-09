#include "CommandProcessor.hpp"

#include <regex>

namespace {

const std::vector<std::string> CommandLines{
    "add <id> <name>                         Add a task with a user-defined ID",
    "add \"id\" \"name\"                   Quoted ID/name form",
    "remove <id | fuzzy name>                Remove a task",
    "start <id | fuzzy name>                 Start or resume a task",
    "stop <id | fuzzy name>                  Stop a running task",
    "done <id | fuzzy name>                  Complete a task",
    "search <fuzzy text>                     Show fuzzy-matched tasks",
    "list                                    Show all tasks",
    "commands                                Show all commands",
    "setJsonFile <path.json>                 Switch active JSON data file",
    "color rgb(r,g,b) bg rgb(r,g,b)         Set default task-row colors",
    "color <id> rgb(r,g,b) bg rgb(r,g,b)    Set colors for one task",
    "help                                    Show short help",
    "quit                                    Save and exit Sentinel"
};

std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t");
    return value.substr(first, last - first + 1);
}

std::string Unquote(std::string value) {
    value = Trim(std::move(value));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

bool ParseAddArguments(const std::string& argument, std::string& id, std::string& name) {
    static const std::regex pattern(R"(^\s*(?:"([^"]+)"|(\S+))\s+(?:"([^"]+)"|(.+?))\s*$)");
    std::smatch match;
    if (!std::regex_match(argument, match, pattern)) return false;
    id = match[1].matched ? match[1].str() : match[2].str();
    name = match[3].matched ? match[3].str() : Trim(match[4].str());
    return !id.empty() && !name.empty();
}

bool ParseRgbValues(const std::smatch& match, int offset, RgbColor& foreground, RgbColor& background) {
    int values[6]{};
    for (int index = 0; index < 6; ++index) {
        values[index] = std::stoi(match[offset + index].str());
        if (values[index] < 0 || values[index] > 255) return false;
    }
    foreground = {values[0], values[1], values[2]};
    background = {values[3], values[4], values[5]};
    return true;
}

} // namespace

CommandProcessor::CommandProcessor(TaskManager& manager, DisplaySettings& displaySettings)
    : manager_(manager), displaySettings_(displaySettings) {}

std::optional<std::size_t> CommandProcessor::ResolveTaskReference(const std::string& value) {
    const std::string reference = Unquote(value);
    if (reference.empty()) {
        status_ = "Expected a task ID or fuzzy task name.";
        return std::nullopt;
    }
    if (const auto exact = manager_.FindIndexById(reference)) return exact;
    const auto fuzzyIndex = manager_.FindBestMatch(reference);
    if (!fuzzyIndex) {
        status_ = "No task matches: " + reference;
        return std::nullopt;
    }
    return fuzzyIndex;
}

void CommandProcessor::Autosave() {
    std::string error;
    if (!manager_.Save(error)) status_ += " | Autosave failed: " + error;
}

bool CommandProcessor::ParseColorCommand(const std::string& argument) {
    static const std::regex defaultPattern(
        R"(^\s*(?:rgb|rpg)\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*\)\s+bg\s+(?:rgb|rpg)\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*\)\s*$)",
        std::regex::icase
    );
    static const std::regex taskPattern(
        R"(^\s*(?:"([^"]+)"|(\S+))\s+(?:rgb|rpg)\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*\)\s+bg\s+(?:rgb|rpg)\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*\)\s*$)",
        std::regex::icase
    );

    std::smatch match;
    RgbColor foreground;
    RgbColor background;

    if (std::regex_match(argument, match, defaultPattern)) {
        if (!ParseRgbValues(match, 1, foreground, background)) {
            status_ = "RGB channels must be between 0 and 255.";
            return false;
        }
        displaySettings_.foreground = foreground;
        displaySettings_.background = background;
        displaySettings_.dirty = true;
        status_ = "Default task colors updated.";
        return true;
    }

    if (std::regex_match(argument, match, taskPattern)) {
        const std::string id = match[1].matched ? match[1].str() : match[2].str();
        if (!ParseRgbValues(match, 3, foreground, background)) {
            status_ = "RGB channels must be between 0 and 255.";
            return false;
        }
        Task* task = manager_.GetTaskById(id);
        if (!task) {
            status_ = "Task ID does not exist: " + id;
            return false;
        }
        task->SetColor(foreground, background);
        status_ = "Task colors updated: " + id;
        Autosave();
        return true;
    }

    status_ = "Usage: color [\"id\"] rgb(r,g,b) bg rgb(r,g,b)";
    return false;
}

void CommandProcessor::Execute(const std::string& line) {
    const auto separator = line.find_first_of(" \t");
    const std::string command = line.substr(0, separator);
    std::string argument = separator == std::string::npos ? std::string{} : Trim(line.substr(separator + 1));

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
        const std::string path = Unquote(argument);
        if (path.empty()) { status_ = "Usage: setJsonFile <json_file_path.json>"; return; }
        std::string error;
        status_ = manager_.SetJsonFile(path, error) ? "JSON data file: " + manager_.GetJsonFile() : "Could not switch JSON file: " + error;
        return;
    }
    if (command == "color") {
        ParseColorCommand(argument);
        return;
    }
    if (command == "add") {
        std::string id;
        std::string name;
        if (!ParseAddArguments(argument, id, name)) {
            status_ = "Usage: add <id> <name> or add \"id\" \"name\"";
            return;
        }
        if (!manager_.AddTask(id, name)) {
            status_ = "Task ID must be unique and non-empty: " + id;
            return;
        }
        status_ = "Task added: " + id + " - " + name;
        Autosave();
        return;
    }
    if (command == "search") {
        const std::string query = Unquote(argument);
        if (query.empty()) { status_ = "Usage: search <fuzzy text>"; return; }
        searchResults_ = manager_.Search(query);
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
        status_ = "Task started: " + task->GetId();
    } else if (command == "stop") {
        task->Stop();
        status_ = "Task stopped: " + task->GetId();
    } else {
        task->Complete();
        status_ = "Task completed: " + task->GetId();
    }
    Autosave();
}

bool CommandProcessor::ShouldQuit() const noexcept { return quit_; }
const std::string& CommandProcessor::GetStatusMessage() const noexcept { return status_; }
const std::optional<std::vector<std::size_t>>& CommandProcessor::GetSearchResults() const noexcept { return searchResults_; }
const std::vector<std::string>& CommandProcessor::GetInfoLines() const noexcept { return infoLines_; }
