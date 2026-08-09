#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class NodeKind {
    Folder,
    Task
};

struct TaskNode {
    std::string id;
    std::string name;
    std::string description;
    std::string parentId;
    NodeKind kind{NodeKind::Task};
    std::vector<std::string> children;
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
