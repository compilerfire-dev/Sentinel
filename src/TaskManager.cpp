#include "TaskManager.hpp"

#include "FuzzySearch.hpp"

#include <algorithm>
#include <utility>

Task& TaskManager::AddTask(std::string name) {
    tasks_.emplace_back(std::move(name));
    return tasks_.back();
}

bool TaskManager::RemoveTask(std::size_t index) {
    if (index >= tasks_.size()) {
        return false;
    }

    tasks_.erase(tasks_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

Task* TaskManager::GetTask(std::size_t index) {
    return index < tasks_.size() ? &tasks_[index] : nullptr;
}

const Task* TaskManager::GetTask(std::size_t index) const {
    return index < tasks_.size() ? &tasks_[index] : nullptr;
}

std::vector<TaskSearchResult> TaskManager::FuzzySearchTasks(
    const std::string& query
) const {
    std::vector<TaskSearchResult> results;

    for (std::size_t index = 0; index < tasks_.size(); ++index) {
        const auto score = FuzzySearch::Score(query, tasks_[index].GetName());
        if (!score) {
            continue;
        }

        results.push_back(TaskSearchResult{index, *score});
    }

    std::stable_sort(
        results.begin(),
        results.end(),
        [](const TaskSearchResult& left, const TaskSearchResult& right) {
            if (left.score != right.score) {
                return left.score > right.score;
            }
            return left.index < right.index;
        }
    );

    return results;
}

std::vector<std::size_t> TaskManager::Search(const std::string& query) const {
    const auto ranked = FuzzySearchTasks(query);
    std::vector<std::size_t> indices;
    indices.reserve(ranked.size());

    for (const auto& result : ranked) {
        indices.push_back(result.index);
    }

    return indices;
}

std::optional<std::size_t> TaskManager::FindBestMatch(
    const std::string& query
) const {
    const auto results = FuzzySearchTasks(query);
    if (results.empty()) {
        return std::nullopt;
    }

    return results.front().index;
}

const std::vector<Task>& TaskManager::GetTasks() const noexcept {
    return tasks_;
}
