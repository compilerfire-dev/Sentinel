#pragma once

#include "DisplaySettings.hpp"
#include "TaskManager.hpp"

#include <optional>
#include <string>
#include <vector>

class CommandProcessor {
public:
    CommandProcessor(TaskManager& manager, DisplaySettings& displaySettings);

    void Execute(const std::string& line);

    bool ShouldQuit() const noexcept;
    const std::string& GetStatusMessage() const noexcept;
    const std::optional<std::vector<std::size_t>>& GetSearchResults() const noexcept;
    const std::vector<std::string>& GetInfoLines() const noexcept;

private:
    std::optional<std::size_t> ResolveTaskReference(const std::string& value);
    bool ParseColorCommand(const std::string& argument);
    void Autosave();

    TaskManager& manager_;
    DisplaySettings& displaySettings_;
    bool quit_{false};
    std::string status_;
    std::optional<std::vector<std::size_t>> searchResults_;
    std::vector<std::string> infoLines_;
};
