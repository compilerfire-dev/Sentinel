#include "TaskDataStore.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
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
    std::string& errorMessage
) const {
    try {
        if (!std::filesystem::exists(path_)) {
            tree = TaskTree{};
            displaySettings = TreeDisplaySettings{};
            definedColors.clear();
            errorMessage.clear();
            return true;
        }

        std::ifstream input(path_);
        if (!input) {
            errorMessage = "Could not open JSON file for reading: " + path_.string();
            return false;
        }

        json root;
        input >> root;
        if (!root.is_object()) {
            errorMessage = "JSON root must be an object.";
            return false;
        }

        TaskTree loadedTree;
        TreeDisplaySettings loadedDisplay;
        std::unordered_map<std::string, RgbColor> loadedColors;

        if (!root.contains("sentinelTasks")) {
            tree = std::move(loadedTree);
            displaySettings = loadedDisplay;
            definedColors = std::move(loadedColors);
            errorMessage.clear();
            return true;
        }

        const json& data = root.at("sentinelTasks");
        if (!data.is_object()) {
            errorMessage = "sentinelTasks must be a JSON object.";
            return false;
        }

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
                const std::string parent = item.value("parent", std::string{});
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
    std::string& errorMessage
) const {
    try {
        if (path_.has_parent_path()) {
            std::filesystem::create_directories(path_.parent_path());
        }

        json root = json::object();
        if (std::filesystem::exists(path_)) {
            std::ifstream input(path_);
            if (!input) {
                errorMessage = "Could not open JSON file before save: " + path_.string();
                return false;
            }
            input >> root;
            if (!root.is_object()) {
                errorMessage = "JSON root must be an object.";
                return false;
            }
        }

        json data;
        data["version"] = 1;
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

        root["sentinelTasks"] = std::move(data);

        const std::filesystem::path temporary = path_.string() + ".tmp";
        {
            std::ofstream output(temporary);
            if (!output) {
                errorMessage = "Could not open temporary JSON file for writing: " + temporary.string();
                return false;
            }
            output << root.dump(4) << '\n';
        }

        std::error_code error;
        std::filesystem::rename(temporary, path_, error);
        if (error) {
            std::filesystem::remove(path_, error);
            error.clear();
            std::filesystem::rename(temporary, path_, error);
        }
        if (error) {
            errorMessage = "Could not replace JSON file: " + error.message();
            return false;
        }

        errorMessage.clear();
        return true;
    } catch (const std::exception& exception) {
        errorMessage = "Failed to save SentinelTasks JSON: " + std::string(exception.what());
        return false;
    }
}

void TaskDataStore::SetPath(std::filesystem::path path) {
    path_ = std::move(path);
}

const std::filesystem::path& TaskDataStore::Path() const noexcept {
    return path_;
}
