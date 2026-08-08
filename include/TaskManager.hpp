#pragma once
#include "Task.hpp"
#include <cstddef>
#include <string>
#include <vector>
class TaskManager { public: Task& AddTask(std::string name); bool RemoveTask(std::size_t index); Task* GetTask(std::size_t index); const Task* GetTask(std::size_t index) const; std::vector<std::size_t> Search(const std::string& query) const; const std::vector<Task>& GetTasks() const noexcept; private: std::vector<Task> tasks_; };
