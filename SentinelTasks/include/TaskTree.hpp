#pragma once

#include "Color.hpp"
#include "TimeFragment.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class NodeKind {
    Folder,
    Task
};

struct TaskNode {
    using Clock = std::chrono::steady_clock;
    using SystemClock = std::chrono::system_clock;

    std::string id;
    std::string name;
    std::string description;
    std::string parentId;
    NodeKind kind{NodeKind::Task};
    std::vector<std::string> children;
    std::optional<RgbColor> foregroundColor;
    std::optional<RgbColor> backgroundColor;
    bool collapsed{false};

    std::chrono::seconds accumulatedTime{0};
    bool running{false};
    bool completed{false};
    Clock::time_point startedAt{};
    SystemClock::time_point createdAt{SystemClock::now()};
    SystemClock::time_point completedAt{};
    std::vector<SentinelShared::TimeFragment> timeFragments;

    void Start();
    void Stop();
    void Complete();
    void Unset();
    void RestoreTiming(
        std::chrono::seconds elapsed,
        bool restoredCompleted,
        bool restoredRunning,
        SystemClock::time_point restoredCreatedAt,
        SystemClock::time_point restoredCompletedAt,
        std::vector<SentinelShared::TimeFragment> fragments
    );
    std::chrono::seconds Elapsed() const;
    std::vector<SentinelShared::TimeFragment> TimeFragments() const;
    std::string ElapsedString() const;
    std::string CreatedString() const;
    std::string CompletionString() const;
};

struct VisibleTreeNode {
    const TaskNode* node{nullptr};
    std::string connectorPrefix;
    std::size_t depth{0};
};

class TaskTree {
public:
    bool AddNode(
        NodeKind kind,
        std::string id,
        std::string parentId,
        std::string name,
        std::string& errorMessage
    );

    bool RemoveNode(const std::string& id, std::string& errorMessage);
    bool RenameNode(const std::string& id, std::string name, std::string& errorMessage);
    bool MoveNode(const std::string& id, std::string parentId, std::string& errorMessage);
    bool SetCollapsed(const std::string& id, bool collapsed, std::string& errorMessage);
    bool ToggleCollapsed(const std::string& id, std::string& errorMessage);
    bool ExpandAncestors(const std::string& id, std::string& errorMessage);
    bool SetDescription(const std::string& id, std::string description, std::string& errorMessage);
    bool SetColor(const std::string& id, RgbColor foreground, RgbColor background, std::string& errorMessage);
    void ClearColor(const std::string& id);

    bool StartTask(const std::string& id, std::string& errorMessage);
    bool StopTask(const std::string& id, std::string& errorMessage);
    bool CompleteTask(const std::string& id, std::string& errorMessage);
    bool UnsetTask(const std::string& id, std::string& errorMessage);

    TaskNode* GetNode(const std::string& id);
    const TaskNode* GetNode(const std::string& id) const;

    // Full traversal is stable for persistence, dialogs and statistics.
    std::vector<VisibleTreeNode> Flatten() const;
    // UI traversal omits descendants of collapsed folders.
    std::vector<VisibleTreeNode> FlattenVisible() const;
    std::optional<std::string> ParentOf(const std::string& id) const;
    std::optional<std::string> FirstChildOf(const std::string& id) const;
    bool Empty() const noexcept;

private:
    void FlattenChildren(
        const std::vector<std::string>& ids,
        const std::string& prefix,
        std::size_t depth,
        bool respectCollapsed,
        std::vector<VisibleTreeNode>& output
    ) const;
    void CollectSubtreeIds(const std::string& id, std::vector<std::string>& output) const;

    std::unordered_map<std::string, TaskNode> nodes_;
    std::vector<std::string> rootIds_;
};
