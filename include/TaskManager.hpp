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

    Task* GetTask(std::size_t index);
    const Task* GetTask(std::size_t index) const;

    std::vector<TaskSearchResult> FuzzySearchTasks(const std::string& query) const;
    std::vector<std::size_t> Search(const std::string& query) const;
    std::optional<std::size_t> FindBestMatch(const std::string& query) const;

    const std::vector<Task>& GetTasks() const noexcept;

private:
    std::vector<Task> tasks_;
};
