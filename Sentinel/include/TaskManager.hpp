#pragma once

#include "Task.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct TaskSearchResult {
    std::size_t index;
    int score;
};

class TaskManager {
public:
    Task* AddTask(std::string id, std::string name);
    bool RemoveTask(std::size_t index);
    void Clear();

    Task* GetTask(std::size_t index);
    const Task* GetTask(std::size_t index) const;
    Task* GetTaskById(const std::string& id);
    const Task* GetTaskById(const std::string& id) const;
    std::optional<std::size_t> FindIndexById(const std::string& id) const;

    std::vector<TaskSearchResult> FuzzySearchTasks(const std::string& query) const;
    std::vector<std::size_t> Search(const std::string& query) const;
    std::optional<std::size_t> FindBestMatch(const std::string& query) const;

    void DefineColor(std::string id, const RgbColor& color);
    std::optional<RgbColor> GetDefinedColor(const std::string& id) const;
    const std::unordered_map<std::string, RgbColor>& GetDefinedColors() const noexcept;

    void SetAutoSaveInterval(std::chrono::seconds interval);
    std::chrono::seconds GetAutoSaveInterval() const noexcept;

    bool Load(std::string& errorMessage);
    bool Save(std::string& errorMessage) const;
    bool SaveNow(std::string& errorMessage) const;
    bool SetJsonFile(const std::string& path, std::string& errorMessage);
    const std::string& GetJsonFile() const noexcept;

    const std::vector<Task>& GetTasks() const noexcept;

private:
    std::vector<Task> tasks_;
    std::unordered_map<std::string, RgbColor> definedColors_;
    std::string jsonFilePath_{"current_data.json"};
    std::chrono::seconds autoSaveInterval_{1};
    mutable std::chrono::steady_clock::time_point lastPeriodicSave_{
        std::chrono::steady_clock::now()
    };
};