#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct TimePointValue {
    std::int64_t epochSeconds{0};
    double value{0.0};
};

struct ProjectSeries {
    std::string id;
    std::string name;
    std::vector<TimePointValue> locHistory;
};

struct StatisticsSnapshot {
    std::size_t totalTasks{0};
    std::size_t completedTasks{0};
    std::size_t runningTasks{0};
    std::uint64_t totalTrackedSeconds{0};
    std::vector<TimePointValue> completedTaskHistory;
    std::vector<ProjectSeries> projects;
};

class StatisticsData {
public:
    bool Load(const std::filesystem::path& path, std::string& errorMessage);

    const StatisticsSnapshot& Snapshot() const noexcept;
    const std::filesystem::path& Path() const noexcept;

private:
    StatisticsSnapshot snapshot_;
    std::filesystem::path path_;
};
