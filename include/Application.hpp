#pragma once

#include "CommandProcessor.hpp"
#include "DisplaySettings.hpp"
#include "TaskManager.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class Application {
public:
    Application();
    int Run();

private:
    struct Suggestion {
        std::string label;
        std::string replacement;
    };

    void HandleInput();
    void HandleMouse();
    void AcceptSuggestion(const std::vector<Suggestion>& suggestions);
    void ResetSuggestionSelection();
    void AddCommandToHistory(const std::string& command);
    void RecallPreviousCommand();
    void RecallNextCommand();
    void ResetHistoryNavigation();
    void PeriodicAutosave();
    void ApplyColors();

    void Render();
    void RenderHeader();
    void RenderTasks();
    void RenderSuggestions(const std::vector<Suggestion>& suggestions);
    void RenderStatus();
    void RenderCommandLine();

    std::vector<Suggestion> BuildSuggestions() const;
    std::vector<std::size_t> VisibleTaskIndices() const;
    int SuggestionStartRow(std::size_t suggestionCount) const;

    static std::string FormatDuration(std::chrono::seconds duration);

    TaskManager taskManager_;
    DisplaySettings displaySettings_;
    CommandProcessor commandProcessor_;
    std::string commandBuffer_;
    std::string persistenceStatus_;
    std::vector<std::string> commandHistory_;
    std::optional<std::size_t> historyIndex_;
    std::string commandBeforeHistory_;
    std::size_t selectedSuggestion_{0};
    bool navigatingSuggestions_{false};
    std::chrono::steady_clock::time_point lastAutosave_{};
};
