#pragma once

#include "Color.hpp"
#include "TaskTree.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>

class TaskDataStore {
public:
    explicit TaskDataStore(std::filesystem::path path = "current_data.json");

    bool Load(
        TaskTree& tree,
        TreeDisplaySettings& displaySettings,
        std::unordered_map<std::string, RgbColor>& definedColors,
        std::chrono::seconds& autoSaveInterval,
        std::string& errorMessage
    ) const;

    bool Save(
        const TaskTree& tree,
        const TreeDisplaySettings& displaySettings,
        const std::unordered_map<std::string, RgbColor>& definedColors,
        std::chrono::seconds autoSaveInterval,
        std::string& errorMessage
    ) const;

    void SetPath(std::filesystem::path path);
    const std::filesystem::path& Path() const noexcept;

private:
    std::filesystem::path path_;
};