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

struct TaskFragment {
    std::int64_t startedAtEpoch{0};
    std::int64_t endedAtEpoch{0};
    std::uint64_t durationSeconds{0};
    bool open{false};
};

struct TaskFragmentSeries {
    std::string id;
    std::string name;
    std::string source;
    std::int64_t createdAtEpoch{0};
    std::vector<TaskFragment> fragments;
};

struct StatisticsSnapshot {
    std::size_t totalTasks{0};
    std::size_t completedTasks{0};
    std::size_t runningTasks{0};
    std::size_t totalFragments{0};
    std::uint64_t totalTrackedSeconds{0};
    std::vector<TimePointValue> createdTaskHistory;
    std::vector<TimePointValue> completedTaskHistory;
    std::vector<TaskFragmentSeries> taskFragments;
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
