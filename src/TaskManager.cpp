#include "TaskManager.hpp"

#include "FuzzySearch.hpp"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <utility>

using nlohmann::json;

Task& TaskManager::AddTask(std::string name) {
    tasks_.emplace_back(std::move(name));
    return tasks_.back();
}

bool TaskManager::RemoveTask(std::size_t index) {
    if (index >= tasks_.size()) return false;
    tasks_.erase(tasks_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

void TaskManager::Clear() { tasks_.clear(); }

Task* TaskManager::GetTask(std::size_t index) {
    return index < tasks_.size() ? &tasks_[index] : nullptr;
}

const Task* TaskManager::GetTask(std::size_t index) const {
    return index < tasks_.size() ? &tasks_[index] : nullptr;
}

std::vector<TaskSearchResult> TaskManager::FuzzySearchTasks(const std::string& query) const {
    std::vector<TaskSearchResult> results;
    for (std::size_t index = 0; index < tasks_.size(); ++index) {
        const auto score = FuzzySearch::Score(query, tasks_[index].GetName());
        if (score) results.push_back({index, *score});
    }
    std::stable_sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.score != b.score ? a.score > b.score : a.index < b.index;
    });
    return results;
}

std::vector<std::size_t> TaskManager::Search(const std::string& query) const {
    const auto ranked = FuzzySearchTasks(query);
    std::vector<std::size_t> indices;
    indices.reserve(ranked.size());
    for (const auto& result : ranked) indices.push_back(result.index);
    return indices;
}

std::optional<std::size_t> TaskManager::FindBestMatch(const std::string& query) const {
    const auto results = FuzzySearchTasks(query);
    return results.empty() ? std::nullopt : std::optional<std::size_t>{results.front().index};
}

bool TaskManager::Save(std::string& errorMessage) const {
    try {
        json root;
        root["version"] = 1;
        root["tasks"] = json::array();

        for (const Task& task : tasks_) {
            const auto completedEpoch = std::chrono::duration_cast<std::chrono::seconds>(
                task.GetCompletionTime().time_since_epoch()
            ).count();

            root["tasks"].push_back({
                {"name", task.GetName()},
                {"elapsed_seconds", task.GetElapsedTime().count()},
                {"running", task.IsRunning()},
                {"completed", task.IsCompleted()},
                {"completed_at_epoch", task.IsCompleted() ? completedEpoch : 0}
            });
        }

        const std::filesystem::path path(jsonFilePath_);
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());

        std::ofstream output(path);
        if (!output) {
            errorMessage = "Could not open JSON file for writing: " + jsonFilePath_;
            return false;
        }
        output << root.dump(4) << '\n';
        errorMessage.clear();
        return true;
    } catch (const std::exception& exception) {
        errorMessage = exception.what();
        return false;
    }
}

bool TaskManager::Load(std::string& errorMessage) {
    try {
        const std::filesystem::path path(jsonFilePath_);
        if (!std::filesystem::exists(path)) {
            tasks_.clear();
            return Save(errorMessage);
        }

        std::ifstream input(path);
        if (!input) {
            errorMessage = "Could not open JSON file for reading: " + jsonFilePath_;
            return false;
        }

        json root;
        input >> root;
        if (!root.contains("tasks") || !root["tasks"].is_array()) {
            errorMessage = "JSON file does not contain a tasks array.";
            return false;
        }

        std::vector<Task> loaded;
        for (const auto& item : root["tasks"]) {
            Task task(item.at("name").get<std::string>());
            const auto elapsed = std::chrono::seconds(item.value("elapsed_seconds", 0LL));
            const bool completed = item.value("completed", false);
            const bool running = item.value("running", false);
            const auto epoch = item.value("completed_at_epoch", 0LL);
            const auto completedAt = Task::SystemClock::time_point(std::chrono::seconds(epoch));
            task.Restore(elapsed, completed, running, completedAt);
            loaded.push_back(std::move(task));
        }

        tasks_ = std::move(loaded);
        errorMessage.clear();
        return true;
    } catch (const std::exception& exception) {
        errorMessage = "Failed to load JSON: " + std::string(exception.what());
        return false;
    }
}

bool TaskManager::SetJsonFile(const std::string& path, std::string& errorMessage) {
    if (path.empty()) {
        errorMessage = "JSON file path cannot be empty.";
        return false;
    }

    std::string saveError;
    if (!Save(saveError)) {
        errorMessage = "Current data could not be saved before switching: " + saveError;
        return false;
    }

    jsonFilePath_ = path;
    tasks_.clear();
    return Load(errorMessage);
}

const std::string& TaskManager::GetJsonFile() const noexcept { return jsonFilePath_; }
const std::vector<Task>& TaskManager::GetTasks() const noexcept { return tasks_; }
