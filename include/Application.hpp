#pragma once

#include "CommandProcessor.hpp"
#include "TaskManager.hpp"

#include <chrono>
#include <cstddef>
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
    CommandProcessor commandProcessor_;
    std::string commandBuffer_;
    std::size_t selectedSuggestion_{0};
    bool navigatingSuggestions_{false};
};
