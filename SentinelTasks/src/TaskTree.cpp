#include "TaskTree.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {

std::string FormatDuration(std::chrono::seconds seconds) {
    const auto total = seconds.count();
    const auto hours = total / 3600;
    const auto minutes = (total % 3600) / 60;
    const auto remaining = total % 60;

    std::ostringstream stream;
    stream << std::setfill('0')
           << std::setw(2) << hours << ':'
           << std::setw(2) << minutes << ':'
           << std::setw(2) << remaining;
    return stream.str();
}

std::string FormatTimePoint(TaskNode::SystemClock::time_point value) {
    if (value.time_since_epoch() == TaskNode::SystemClock::duration::zero()) return "-";
    const std::time_t time = TaskNode::SystemClock::to_time_t(value);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::ostringstream stream;
    stream << std::put_time(&local, "%Y-%m-%d %H:%M");
    return stream.str();
}

} // namespace

void TaskNode::Start() {
    if (kind != NodeKind::Task || running || completed) return;
    startedAt = Clock::now();
    running = true;

    SentinelShared::TimeFragment fragment;
    fragment.startedAt = SystemClock::now();
    timeFragments.push_back(fragment);
}

void TaskNode::Stop() {
    if (kind != NodeKind::Task || !running) return;

    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        Clock::now() - startedAt
    );
    accumulatedTime += elapsed;
    running = false;

    if (!timeFragments.empty() && timeFragments.back().IsOpen()) {
        timeFragments.back().endedAt = SystemClock::now();
        timeFragments.back().duration = elapsed;
    }
}

void TaskNode::Complete() {
    if (kind != NodeKind::Task || completed) return;
    Stop();
    completed = true;
    completedAt = SystemClock::now();
}

void TaskNode::Unset() {
    if (kind != NodeKind::Task || !completed) return;
    completed = false;
    running = false;
    completedAt = SystemClock::time_point{};
}

void TaskNode::RestoreTiming(
    std::chrono::seconds elapsed,
    bool restoredCompleted,
    bool restoredRunning,
    SystemClock::time_point restoredCreatedAt,
    SystemClock::time_point restoredCompletedAt,
    std::vector<SentinelShared::TimeFragment> fragments
) {
    accumulatedTime = elapsed;
    completed = restoredCompleted;
    running = false;
    createdAt = restoredCreatedAt;
    completedAt = restoredCompletedAt;
    timeFragments = std::move(fragments);

    for (auto& fragment : timeFragments) {
        if (fragment.IsOpen()) {
            fragment.endedAt = fragment.startedAt + fragment.duration;
        }
    }

    if (restoredRunning && !completed) Start();
}

std::chrono::seconds TaskNode::Elapsed() const {
    if (kind != NodeKind::Task) return std::chrono::seconds{0};
    if (!running) return accumulatedTime;
    return accumulatedTime + std::chrono::duration_cast<std::chrono::seconds>(
        Clock::now() - startedAt
    );
}

std::vector<SentinelShared::TimeFragment> TaskNode::TimeFragments() const {
    auto fragments = timeFragments;
    if (running && !fragments.empty() && fragments.back().IsOpen()) {
        fragments.back().duration = std::chrono::duration_cast<std::chrono::seconds>(
            Clock::now() - startedAt
        );
    }
    return fragments;
}

std::string TaskNode::ElapsedString() const {
    return FormatDuration(Elapsed());
}

std::string TaskNode::CreatedString() const {
    return FormatTimePoint(createdAt);
}

std::string TaskNode::CompletionString() const {
    return completed ? FormatTimePoint(completedAt) : "-";
}

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

bool TaskTree::SetColor(const std::string& id, RgbColor foreground, RgbColor background, std::string& errorMessage) {
    TaskNode* node = GetNode(id);
    if (!node) {
        errorMessage = "Node ID does not exist: " + id;
        return false;
    }
    node->foregroundColor = foreground;
    node->backgroundColor = background;
    errorMessage.clear();
    return true;
}

void TaskTree::ClearColor(const std::string& id) {
    if (TaskNode* node = GetNode(id)) {
        node->foregroundColor.reset();
        node->backgroundColor.reset();
    }
}

bool TaskTree::StartTask(const std::string& id, std::string& errorMessage) {
    TaskNode* node = GetNode(id);
    if (!node) { errorMessage = "Node ID does not exist: " + id; return false; }
    if (node->kind != NodeKind::Task) { errorMessage = "Timers can only be started on task nodes."; return false; }
    if (node->completed) { errorMessage = "Completed tasks cannot be restarted. Use unset first."; return false; }
    node->Start();
    errorMessage.clear();
    return true;
}

bool TaskTree::StopTask(const std::string& id, std::string& errorMessage) {
    TaskNode* node = GetNode(id);
    if (!node) { errorMessage = "Node ID does not exist: " + id; return false; }
    if (node->kind != NodeKind::Task) { errorMessage = "Timers can only be stopped on task nodes."; return false; }
    node->Stop();
    errorMessage.clear();
    return true;
}

bool TaskTree::CompleteTask(const std::string& id, std::string& errorMessage) {
    TaskNode* node = GetNode(id);
    if (!node) { errorMessage = "Node ID does not exist: " + id; return false; }
    if (node->kind != NodeKind::Task) { errorMessage = "Only task nodes can be completed."; return false; }
    node->Complete();
    errorMessage.clear();
    return true;
}

bool TaskTree::UnsetTask(const std::string& id, std::string& errorMessage) {
    TaskNode* node = GetNode(id);
    if (!node) { errorMessage = "Node ID does not exist: " + id; return false; }
    if (node->kind != NodeKind::Task) { errorMessage = "Only task nodes can be unset."; return false; }
    if (!node->completed) { errorMessage = "Task is already unset: " + id; return false; }
    node->Unset();
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
