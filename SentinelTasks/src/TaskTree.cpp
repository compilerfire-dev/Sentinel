#include "TaskTree.hpp"

#include <algorithm>
#include <utility>

bool TaskTree::AddNode(
    NodeKind kind,
    std::string id,
    std::string parentId,
    std::string name,
    std::string& errorMessage
) {
    if (id.empty()) {
        errorMessage = "Node ID cannot be empty.";
        return false;
    }
    if (name.empty()) {
        errorMessage = "Node name cannot be empty.";
        return false;
    }
    if (nodes_.contains(id)) {
        errorMessage = "Node ID already exists: " + id;
        return false;
    }

    const bool root = parentId.empty() || parentId == "root" || parentId == "/";
    if (!root) {
        TaskNode* parent = GetNode(parentId);
        if (!parent) {
            errorMessage = "Parent ID does not exist: " + parentId;
            return false;
        }
        if (parent->kind != NodeKind::Folder) {
            errorMessage = "Parent must be a folder: " + parentId;
            return false;
        }
    } else {
        parentId.clear();
    }

    TaskNode node;
    node.id = std::move(id);
    node.name = std::move(name);
    node.parentId = parentId;
    node.kind = kind;

    const std::string newId = node.id;
    nodes_.emplace(newId, std::move(node));

    if (parentId.empty()) rootIds_.push_back(newId);
    else nodes_.at(parentId).children.push_back(newId);

    errorMessage.clear();
    return true;
}

bool TaskTree::RemoveNode(const std::string& id, std::string& errorMessage) {
    const TaskNode* node = GetNode(id);
    if (!node) {
        errorMessage = "Node ID does not exist: " + id;
        return false;
    }

    const std::string parentId = node->parentId;
    std::vector<std::string> subtree;
    CollectSubtreeIds(id, subtree);

    auto& siblings = parentId.empty() ? rootIds_ : nodes_.at(parentId).children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
    for (const auto& subtreeId : subtree) nodes_.erase(subtreeId);

    errorMessage.clear();
    return true;
}

bool TaskTree::SetDescription(const std::string& id, std::string description, std::string& errorMessage) {
    TaskNode* node = GetNode(id);
    if (!node) {
        errorMessage = "Node ID does not exist: " + id;
        return false;
    }
    node->description = std::move(description);
    errorMessage.clear();
    return true;
}

TaskNode* TaskTree::GetNode(const std::string& id) {
    const auto it = nodes_.find(id);
    return it == nodes_.end() ? nullptr : &it->second;
}

const TaskNode* TaskTree::GetNode(const std::string& id) const {
    const auto it = nodes_.find(id);
    return it == nodes_.end() ? nullptr : &it->second;
}

std::vector<VisibleTreeNode> TaskTree::Flatten() const {
    std::vector<VisibleTreeNode> output;
    FlattenChildren(rootIds_, "", 0, output);
    return output;
}

void TaskTree::FlattenChildren(
    const std::vector<std::string>& ids,
    const std::string& prefix,
    std::size_t depth,
    std::vector<VisibleTreeNode>& output
) const {
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const bool last = i + 1 == ids.size();
        const TaskNode* node = GetNode(ids[i]);
        if (!node) continue;
        output.push_back({node, prefix + (last ? "`- " : "+- "), depth});
        FlattenChildren(node->children, prefix + (last ? "   " : "|  "), depth + 1, output);
    }
}

void TaskTree::CollectSubtreeIds(const std::string& id, std::vector<std::string>& output) const {
    const TaskNode* node = GetNode(id);
    if (!node) return;
    for (const auto& child : node->children) CollectSubtreeIds(child, output);
    output.push_back(id);
}

std::optional<std::string> TaskTree::ParentOf(const std::string& id) const {
    const TaskNode* node = GetNode(id);
    if (!node || node->parentId.empty()) return std::nullopt;
    return node->parentId;
}

std::optional<std::string> TaskTree::FirstChildOf(const std::string& id) const {
    const TaskNode* node = GetNode(id);
    if (!node || node->children.empty()) return std::nullopt;
    return node->children.front();
}

bool TaskTree::Empty() const noexcept {
    return nodes_.empty();
}
