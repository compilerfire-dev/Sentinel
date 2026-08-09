#pragma once

#include "TaskTree.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class Application {
public:
    int Run();

private:
    struct Suggestion {
        std::string label;
        std::string replacement;
    };

    void HandleInput();
    void HandleMouse();
    void ExecuteCommand(const std::string& commandLine);

    void Render();
    void RenderHeader();
    void RenderTree();
    void RenderDescriptionPane();
    void RenderSuggestions(const std::vector<Suggestion>& suggestions);
    void RenderStatus();
    void RenderCommandLine();

    std::vector<Suggestion> BuildSuggestions() const;
    void AcceptSuggestion(const std::vector<Suggestion>& suggestions);
    void ResetSuggestionNavigation();

    void AddCommandToHistory(const std::string& command);
    void RecallPreviousCommand();
    void RecallNextCommand();
    void ResetHistoryNavigation();

    void EnterManualSelect(const std::optional<std::string>& requestedId = std::nullopt);
    void LeaveManualSelect();
    void MoveManualSelection(int delta);
    void SelectParent();
    void SelectFirstChild();
    void EnsureSelection();

    static std::vector<std::string> Tokenize(const std::string& line);
    static std::string JoinTokens(const std::vector<std::string>& tokens, std::size_t start);
    static std::size_t PreviousUtf8Boundary(const std::string& text, std::size_t position);
    static std::size_t NextUtf8Boundary(const std::string& text, std::size_t position);

    TaskTree tree_;
    bool running_{true};
    bool manualSelect_{false};
    std::string selectedId_;
    std::vector<std::string> visibleRowIds_;

    std::string commandBuffer_;
    std::size_t cursorPosition_{0};
    std::string status_;
    std::vector<std::string> infoLines_;

    std::vector<std::string> commandHistory_;
    std::optional<std::size_t> historyIndex_;
    std::string commandBeforeHistory_;

    std::size_t selectedSuggestion_{0};
    bool navigatingSuggestions_{false};
};
