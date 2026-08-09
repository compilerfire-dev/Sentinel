#pragma once

#include "Task.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

struct TaskSearchResult {
    std::size_t index;
    int score;
};

class TaskManager {
public:
    Task& AddTask(std::string name);
    bool RemoveTask(std::size_t index);
    void Clear();

    Task* GetTask(std::size_t index);
    const Task* GetTask(std::size_t index) const;

    std::vector<TaskSearchResult> FuzzySearchTasks(const std::string& query) const;
    std::vector<std::size_t> Search(const std::string& query) const;
    std::optional<std::size_t> FindBestMatch(const std::string& query) const;

    bool Load(std::string& errorMessage);
    bool Save(std::string& errorMessage) const;
    bool SetJsonFile(const std::string& path, std::string& errorMessage);
    const std::string& GetJsonFile() const noexcept;

    const std::vector<Task>& GetTasks() const noexcept;

private:
    std::vector<Task> tasks_;
    std::string jsonFilePath_{"current_data.json"};
};
