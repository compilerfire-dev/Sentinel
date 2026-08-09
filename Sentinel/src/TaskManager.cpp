#include "TaskManager.hpp"

#include "FuzzySearch.hpp"
#include "JsonDataStore.hpp"
#include "SharedTaskWorld.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <utility>

using nlohmann::json;

namespace {

json ColorToJson(const RgbColor& color) {
    return {{"red", color.red}, {"green", color.green}, {"blue", color.blue}};
}

RgbColor ColorFromJson(const json& value) {
    return {
        value.value("red", 255),
        value.value("green", 255),
        value.value("blue", 255)
    };
}

json FragmentToJson(const SentinelShared::TimeFragment& fragment) {
    return {
        {"started_at_epoch", SentinelShared::EpochSeconds(fragment.startedAt)},
        {"ended_at_epoch", fragment.IsOpen() ? 0 : SentinelShared::EpochSeconds(fragment.endedAt)},
        {"duration_seconds", fragment.duration.count()}
    };
}

SentinelShared::TimeFragment FragmentFromJson(const json& value) {
    SentinelShared::TimeFragment fragment;
    const auto started = value.value("started_at_epoch", 0LL);
    const auto ended = value.value("ended_at_epoch", 0LL);
    const auto duration = std::max(0LL, value.value("duration_seconds", 0LL));
    fragment.startedAt = SentinelShared::TimePointFromEpoch(started);
    fragment.endedAt = ended > 0
        ? SentinelShared::TimePointFromEpoch(ended)
        : SentinelShared::TimeFragment::Clock::time_point{};
    fragment.duration = std::chrono::seconds(duration);
    return fragment;
}

json TaskToJson(const Task& task) {
    const auto completedEpoch = std::chrono::duration_cast<std::chrono::seconds>(
        task.GetCompletionTime().time_since_epoch()
    ).count();
    const auto createdEpoch = std::chrono::duration_cast<std::chrono::seconds>(
        task.GetCreatedTime().time_since_epoch()
    ).count();

    json item = {
        {"id", task.GetId()},
        {"name", task.GetName()},
        {"created_at_epoch", createdEpoch},
        {"elapsed_seconds", task.GetElapsedTime().count()},
        {"running", task.IsRunning()},
        {"completed", task.IsCompleted()},
        {"completed_at_epoch", task.IsCompleted() ? completedEpoch : 0},
        {"time_fragments", json::array()}
    };

    for (const auto& fragment : task.GetTimeFragments()) {
        item["time_fragments"].push_back(FragmentToJson(fragment));
    }

    if (task.HasCustomColor()) {
        item["color"] = {
            {"foreground", ColorToJson(*task.GetForegroundColor())},
            {"background", ColorToJson(*task.GetBackgroundColor())}
        };
    }

    return item;
}

bool TaskFromJson(const json& item, Task& task, std::string& errorMessage) {
    try {
        const auto elapsed = std::chrono::seconds(
            item.value("elapsed_seconds", 0LL)
        );
        const bool completed = item.value("completed", false);
        const bool running = item.value("running", false);
        const auto createdEpoch = item.value("created_at_epoch", 0LL);
        const auto completedEpoch = item.value("completed_at_epoch", 0LL);

        std::vector<SentinelShared::TimeFragment> fragments;
        if (item.contains("time_fragments") && item["time_fragments"].is_array()) {
            for (const auto& fragmentJson : item["time_fragments"]) {
                if (fragmentJson.is_object()) {
                    fragments.push_back(FragmentFromJson(fragmentJson));
                }
            }
        }

        task.Restore(
            elapsed,
            completed,
            running,
            SentinelShared::TimePointFromEpoch(createdEpoch),
            SentinelShared::TimePointFromEpoch(completedEpoch),
            std::move(fragments)
        );

        if (item.contains("color") && item["color"].is_object()) {
            const auto& color = item["color"];
            if (color.contains("foreground") && color.contains("background")) {
                task.SetColor(
                    ColorFromJson(color.at("foreground")),
                    ColorFromJson(color.at("background"))
                );
            }
        }

        errorMessage.clear();
        return true;
    } catch (const std::exception& exception) {
        errorMessage = "Invalid shared task: " + std::string(exception.what());
        return false;
    }
}

json& SentinelSettingsForWrite(json& root) {
    if (!root.contains("sentinel") || !root["sentinel"].is_object()) {
        root["sentinel"] = json::object();
    }
    return root["sentinel"];
}

} // namespace

Task* TaskManager::AddTask(std::string id, std::string name) {
    if (id.empty() || name.empty() || GetTaskById(id)) return nullptr;
    tasks_.emplace_back(std::move(id), std::move(name));
    return &tasks_.back();
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

Task* TaskManager::GetTaskById(const std::string& id) {
    const auto index = FindIndexById(id);
    return index ? &tasks_[*index] : nullptr;
}

const Task* TaskManager::GetTaskById(const std::string& id) const {
    const auto index = FindIndexById(id);
    return index ? &tasks_[*index] : nullptr;
}

std::optional<std::size_t> TaskManager::FindIndexById(const std::string& id) const {
    for (std::size_t i = 0; i < tasks_.size(); ++i) {
        if (tasks_[i].GetId() == id) return i;
    }
    return std::nullopt;
}

std::vector<TaskSearchResult> TaskManager::FuzzySearchTasks(const std::string& query) const {
    std::vector<TaskSearchResult> results;
    for (std::size_t index = 0; index < tasks_.size(); ++index) {
        const Task& task = tasks_[index];
        const auto nameScore = FuzzySearch::Score(query, task.GetName());
        const auto idScore = FuzzySearch::Score(query, task.GetId());
        if (!nameScore && !idScore) continue;
        results.push_back({
            index,
            std::max(nameScore.value_or(-100000), idScore.value_or(-100000) + 20)
        });
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
    if (const auto exact = FindIndexById(query)) return exact;
    const auto results = FuzzySearchTasks(query);
    return results.empty()
        ? std::nullopt
        : std::optional<std::size_t>{results.front().index};
}

void TaskManager::DefineColor(std::string id, const RgbColor& color) {
    definedColors_[std::move(id)] = color;
}

std::optional<RgbColor> TaskManager::GetDefinedColor(const std::string& id) const {
    const auto iterator = definedColors_.find(id);
    if (iterator == definedColors_.end()) return std::nullopt;
    return iterator->second;
}

const std::unordered_map<std::string, RgbColor>& TaskManager::GetDefinedColors() const noexcept {
    return definedColors_;
}

void TaskManager::SetAutoSaveInterval(std::chrono::seconds interval) {
    if (interval < std::chrono::seconds(1)) interval = std::chrono::seconds(1);
    autoSaveInterval_ = interval;
    lastPeriodicSave_ = std::chrono::steady_clock::now();
}

std::chrono::seconds TaskManager::GetAutoSaveInterval() const noexcept {
    return autoSaveInterval_;
}

bool TaskManager::Save(std::string& errorMessage) const {
    const auto now = std::chrono::steady_clock::now();
    if (now - lastPeriodicSave_ < autoSaveInterval_) {
        errorMessage.clear();
        return true;
    }
    return SaveNow(errorMessage);
}

bool TaskManager::SaveNow(std::string& errorMessage) const {
    const std::filesystem::path path(jsonFilePath_);

    const bool saved = SentinelShared::JsonDataStore::Update(
        path,
        [&](json& root, std::string& mutationError) {
            if (!root.contains("sharedTasks") || !root["sharedTasks"].is_object()) {
                root["sharedTasks"] = json::object();
            }
            auto& shared = root["sharedTasks"];
            shared["version"] = SentinelShared::SharedTaskWorldVersion;
            shared["tasks"] = json::array();
            for (const Task& task : tasks_) {
                shared["tasks"].push_back(TaskToJson(task));
            }

            auto& settings = SentinelSettingsForWrite(root);
            settings["version"] = 5;
            settings["auto_save_seconds"] = autoSaveInterval_.count();
            settings["defined_colors"] = json::object();
            settings.erase("tasks");
            for (const auto& [id, color] : definedColors_) {
                settings["defined_colors"][id] = ColorToJson(color);
            }

            mutationError.clear();
            return true;
        },
        errorMessage
    );

    if (saved) lastPeriodicSave_ = std::chrono::steady_clock::now();
    return saved;
}

bool TaskManager::Load(std::string& errorMessage) {
    try {
        const std::filesystem::path path(jsonFilePath_);

        bool purgedLegacyWorld = false;
        if (!SentinelShared::EnsureSharedTaskWorld(
                path,
                purgedLegacyWorld,
                errorMessage)) {
            return false;
        }

        json root;
        bool exists = false;
        if (!SentinelShared::JsonDataStore::Read(path, root, exists, errorMessage)) {
            return false;
        }
        if (!exists || !root.contains("sharedTasks") ||
            !root["sharedTasks"].is_object() ||
            !root["sharedTasks"].contains("tasks") ||
            !root["sharedTasks"]["tasks"].is_array()) {
            errorMessage = "Shared task world is missing or invalid.";
            return false;
        }

        std::vector<Task> loaded;
        const auto& sharedTasks = root["sharedTasks"]["tasks"];
        loaded.reserve(sharedTasks.size());

        for (const auto& item : sharedTasks) {
            if (!item.is_object()) continue;
            const std::string id = item.value("id", std::string{});
            const std::string name = item.value("name", id);
            if (id.empty() || name.empty()) {
                errorMessage = "Shared tasks require non-empty id and name.";
                return false;
            }
            if (std::any_of(
                    loaded.begin(), loaded.end(),
                    [&](const Task& task) { return task.GetId() == id; })) {
                errorMessage = "Duplicate global task ID: " + id;
                return false;
            }

            Task task(id, name);
            std::string taskError;
            if (!TaskFromJson(item, task, taskError)) {
                errorMessage = taskError;
                return false;
            }
            loaded.push_back(std::move(task));
        }

        std::unordered_map<std::string, RgbColor> loadedColors;
        std::chrono::seconds loadedAutoSaveInterval{1};
        if (root.contains("sentinel") && root["sentinel"].is_object()) {
            const auto& settings = root["sentinel"];
            loadedAutoSaveInterval = std::chrono::seconds(
                std::max(1LL, settings.value("auto_save_seconds", 1LL))
            );
            if (settings.contains("defined_colors") && settings["defined_colors"].is_object()) {
                for (auto iterator = settings["defined_colors"].begin();
                     iterator != settings["defined_colors"].end(); ++iterator) {
                    loadedColors[iterator.key()] = ColorFromJson(iterator.value());
                }
            }
        }

        tasks_ = std::move(loaded);
        definedColors_ = std::move(loadedColors);
        autoSaveInterval_ = loadedAutoSaveInterval;
        lastPeriodicSave_ = std::chrono::steady_clock::now();
        errorMessage.clear();
        (void)purgedLegacyWorld;
        return true;
    } catch (const std::exception& exception) {
        errorMessage = "Failed to load shared task world: " + std::string(exception.what());
        return false;
    }
}

bool TaskManager::SetJsonFile(const std::string& path, std::string& errorMessage) {
    if (path.empty()) {
        errorMessage = "JSON file path cannot be empty.";
        return false;
    }

    std::string saveError;
    if (!SaveNow(saveError)) {
        errorMessage = "Current data could not be saved before switching: " + saveError;
        return false;
    }

    const std::string previousPath = jsonFilePath_;
    const std::vector<Task> previousTasks = tasks_;
    const auto previousColors = definedColors_;
    const auto previousAutoSaveInterval = autoSaveInterval_;

    jsonFilePath_ = path;
    if (Load(errorMessage)) return true;

    jsonFilePath_ = previousPath;
    tasks_ = previousTasks;
    definedColors_ = previousColors;
    autoSaveInterval_ = previousAutoSaveInterval;
    lastPeriodicSave_ = std::chrono::steady_clock::now();
    return false;
}

const std::string& TaskManager::GetJsonFile() const noexcept {
    return jsonFilePath_;
}

const std::vector<Task>& TaskManager::GetTasks() const noexcept {
    return tasks_;
}
