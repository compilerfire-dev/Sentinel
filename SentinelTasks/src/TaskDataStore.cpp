#include "TaskDataStore.hpp"

#include "JsonDataStore.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <utility>

using nlohmann::json;

namespace {

json ColorToJson(const RgbColor& color) {
    return {{"red", color.red}, {"green", color.green}, {"blue", color.blue}};
}

RgbColor ColorFromJson(const json& value, const RgbColor& fallback) {
    return {
        value.value("red", fallback.red),
        value.value("green", fallback.green),
        value.value("blue", fallback.blue)
    };
}

} // namespace

TaskDataStore::TaskDataStore(std::filesystem::path path)
    : path_(std::move(path)) {}

bool TaskDataStore::Load(
    TaskTree& tree,
    TreeDisplaySettings& displaySettings,
    std::unordered_map<std::string, RgbColor>& definedColors,
    std::chrono::seconds& autoSaveInterval,
    std::string& errorMessage
) const {
    try {
        json root;
        bool exists = false;
        if (!SentinelShared::JsonDataStore::Read(path_, root, exists, errorMessage)) {
            return false;
        }

        TaskTree loadedTree;
        TreeDisplaySettings loadedDisplay;
        std::unordered_map<std::string, RgbColor> loadedColors;
        std::chrono::seconds loadedAutoSaveInterval{1};

        if (!exists || !root.contains("sentinelTasks")) {
            tree = std::move(loadedTree);
            displaySettings = loadedDisplay;
            definedColors = std::move(loadedColors);
            autoSaveInterval = loadedAutoSaveInterval;
            errorMessage.clear();
            return true;
        }

        const json& data = root.at("sentinelTasks");
        if (!data.is_object()) {
            errorMessage = "sentinelTasks must be a JSON object.";
            return false;
        }

        loadedAutoSaveInterval = std::chrono::seconds(
            std::max(1LL, data.value("auto_save_seconds", 1LL))
        );

        if (data.contains("display") && data["display"].is_object()) {
            const auto& display = data["display"];
            if (display.contains("foreground")) {
                loadedDisplay.foreground = ColorFromJson(
                    display["foreground"], loadedDisplay.foreground
                );
            }
            if (display.contains("background")) {
                loadedDisplay.background = ColorFromJson(
                    display["background"], loadedDisplay.background
                );
            }
        }

        if (data.contains("defined_colors") && data["defined_colors"].is_object()) {
            for (auto iterator = data["defined_colors"].begin();
                 iterator != data["defined_colors"].end(); ++iterator) {
                loadedColors[iterator.key()] = ColorFromJson(
                    iterator.value(), RgbColor{255, 255, 255}
                );
            }
        }

        if (data.contains("nodes")) {
            if (!data["nodes"].is_array()) {
                errorMessage = "sentinelTasks.nodes must be an array.";
                return false;
            }

            for (const auto& item : data["nodes"]) {
                const std::string id = item.at("id").get<std::string>();
                const std::string name = item.value("name", id);
                std::string parent;
                if (item.contains("parent") && item["parent"].is_string()) {
                    parent = item["parent"].get<std::string>();
                }
                const std::string type = item.value("type", std::string{"task"});
                const NodeKind kind = type == "folder" ? NodeKind::Folder : NodeKind::Task;

                std::string addError;
                if (!loadedTree.AddNode(kind, id, parent, name, addError)) {
                    errorMessage = "Could not load node '" + id + "': " + addError;
                    return false;
                }

                TaskNode* node = loadedTree.GetNode(id);
                if (!node) continue;

                node->description = item.value("description", std::string{});

                if (item.contains("color") && item["color"].is_object()) {
                    const auto& color = item["color"];
                    if (color.contains("foreground") && color.contains("background")) {
                        node->foregroundColor = ColorFromJson(
                            color["foreground"], RgbColor{255, 255, 255}
                        );
                        node->backgroundColor = ColorFromJson(
                            color["background"], RgbColor{0, 0, 0}
                        );
                    }
                }

                if (kind == NodeKind::Task) {
                    node->accumulatedTime = std::chrono::seconds(
                        item.value("elapsed_seconds", 0LL)
                    );
                    node->completed = item.value("completed", false);
                    node->running = item.value("running", false) && !node->completed;
                    if (node->running) node->startedAt = TaskNode::Clock::now();

                    const auto completedEpoch = item.value("completed_at_epoch", 0LL);
                    if (node->completed && completedEpoch > 0) {
                        node->completedAt = std::chrono::system_clock::time_point(
                            std::chrono::seconds(completedEpoch)
                        );
                    }
                }
            }
        }

        tree = std::move(loadedTree);
        displaySettings = loadedDisplay;
        definedColors = std::move(loadedColors);
        autoSaveInterval = loadedAutoSaveInterval;
        errorMessage.clear();
        return true;
    } catch (const std::exception& exception) {
        errorMessage = "Failed to load SentinelTasks JSON: " + std::string(exception.what());
        return false;
    }
}

bool TaskDataStore::Save(
    const TaskTree& tree,
    const TreeDisplaySettings& displaySettings,
    const std::unordered_map<std::string, RgbColor>& definedColors,
    std::chrono::seconds autoSaveInterval,
    std::string& errorMessage
) const {
    json data;
    data["version"] = 1;
    data["auto_save_seconds"] = std::max<long long>(1, autoSaveInterval.count());
    data["display"] = {
        {"foreground", ColorToJson(displaySettings.foreground)},
        {"background", ColorToJson(displaySettings.background)}
    };
    data["defined_colors"] = json::object();
    for (const auto& [id, color] : definedColors) {
        data["defined_colors"][id] = ColorToJson(color);
    }

    data["nodes"] = json::array();
    for (const auto& visible : tree.Flatten()) {
        if (!visible.node) continue;
        const TaskNode& node = *visible.node;

        json item = {
            {"id", node.id},
            {"name", node.name},
            {"description", node.description},
            {"parent", node.parentId},
            {"type", node.kind == NodeKind::Folder ? "folder" : "task"}
        };

        if (node.foregroundColor && node.backgroundColor) {
            item["color"] = {
                {"foreground", ColorToJson(*node.foregroundColor)},
                {"background", ColorToJson(*node.backgroundColor)}
            };
        }

        if (node.kind == NodeKind::Task) {
            const auto completedEpoch = node.completed
                ? std::chrono::duration_cast<std::chrono::seconds>(
                    node.completedAt.time_since_epoch()
                ).count()
                : 0LL;

            item["elapsed_seconds"] = node.Elapsed().count();
            item["running"] = node.running;
            item["completed"] = node.completed;
            item["completed_at_epoch"] = completedEpoch;
        }

        data["nodes"].push_back(std::move(item));
    }

    return SentinelShared::JsonDataStore::Update(
        path_,
        [data = std::move(data)](json& root, std::string& mutationError) mutable {
            root["sentinelTasks"] = std::move(data);
            mutationError.clear();
            return true;
        },
        errorMessage
    );
}

void TaskDataStore::SetPath(std::filesystem::path path) {
    path_ = std::move(path);
}

const std::filesystem::path& TaskDataStore::Path() const noexcept {
    return path_;
}
