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

    enum class DialogFieldKind {
        TextInput,
        DropList
    };

    struct DialogField {
        std::string label;
        DialogFieldKind kind{DialogFieldKind::TextInput};
        std::string value;
        std::size_t cursor{0};
        std::vector<std::string> options;
        std::size_t selectedOption{0};
        bool dropdownOpen{false};
    };

    struct CommandDialog {
        std::string command;
        std::string title;
        std::vector<DialogField> fields;
        std::size_t focusedControl{0};
        std::string validationMessage;
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
    void RenderCommandDialog();

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

    bool OpenCommandDialog(const std::string& command);
    void CloseCommandDialog();
    void HandleCommandDialogInput(int key);
    void HandleCommandDialogMouse(int mouseX, int mouseY);
    void MoveDialogFocus(int delta);
    void OpenFocusedDropList();
    void CloseFocusedDropList(bool acceptSelection);
    bool SubmitCommandDialog();
    std::string BuildDialogCommand() const;
    std::vector<std::string> NodeIdOptions(bool foldersOnly, bool includeRoot) const;
    std::optional<std::size_t> FindOptionIndex(
        const std::vector<std::string>& options,
        const std::string& value
    ) const;

    static std::string QuoteArgument(const std::string& value);
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

    std::optional<CommandDialog> commandDialog_;
};
