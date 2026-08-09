#pragma once

#include "Color.hpp"

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

    std::string id;
    std::string name;
    std::string description;
    std::string parentId;
    NodeKind kind{NodeKind::Task};
    std::vector<std::string> children;
    std::optional<RgbColor> foregroundColor;
    std::optional<RgbColor> backgroundColor;

    std::chrono::seconds accumulatedTime{0};
    bool running{false};
    bool completed{false};
    Clock::time_point startedAt{};
    std::chrono::system_clock::time_point completedAt{};

    void Start();
    void Stop();
    void Complete();
    std::chrono::seconds Elapsed() const;
    std::string ElapsedString() const;
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
    bool SetDescription(const std::string& id, std::string description, std::string& errorMessage);
    bool SetColor(const std::string& id, RgbColor foreground, RgbColor background, std::string& errorMessage);
    void ClearColor(const std::string& id);

    bool StartTask(const std::string& id, std::string& errorMessage);
    bool StopTask(const std::string& id, std::string& errorMessage);
    bool CompleteTask(const std::string& id, std::string& errorMessage);

    TaskNode* GetNode(const std::string& id);
    const TaskNode* GetNode(const std::string& id) const;

    std::vector<VisibleTreeNode> Flatten() const;
    std::optional<std::string> ParentOf(const std::string& id) const;
    std::optional<std::string> FirstChildOf(const std::string& id) const;
    bool Empty() const noexcept;

private:
    void FlattenChildren(
        const std::vector<std::string>& ids,
        const std::string& prefix,
        std::size_t depth,
        std::vector<VisibleTreeNode>& output
    ) const;
    void CollectSubtreeIds(const std::string& id, std::vector<std::string>& output) const;

    std::unordered_map<std::string, TaskNode> nodes_;
    std::vector<std::string> rootIds_;
};
