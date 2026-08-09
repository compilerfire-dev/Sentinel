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

    enum class DialogFieldKind { TextInput, DropList };

    struct DialogField {
        std::string label;
        DialogFieldKind kind{DialogFieldKind::TextInput};
        std::string value;
        std::size_t cursor{0};
        std::vector<std::string> options;
        std::size_t selectedOption{0};
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
    void AcceptSuggestion(const std::vector<Suggestion>& suggestions);
    void ResetSuggestionSelection();
    void AddCommandToHistory(const std::string& command);
    void RecallPreviousCommand();
    void RecallNextCommand();
    void ResetHistoryNavigation();
    void PeriodicAutosave();
    void ApplyColors();
    void OpenNativeJsonFilePicker();

    bool OpenCommandDialog(const std::string& command);
    void CloseCommandDialog();
    void HandleCommandDialogInput(int key);
    void HandleCommandDialogMouse(int mouseX, int mouseY);
    void MoveDialogFocus(int delta);
    bool SubmitCommandDialog();
    std::string BuildDialogCommand() const;
    std::vector<std::string> TaskIdOptions() const;
    std::vector<std::string> ColorNameOptions() const;
    static std::string QuoteArgument(const std::string& value);

    void Render();
    void RenderHeader();
    void RenderTasks();
    void RenderSuggestions(const std::vector<Suggestion>& suggestions);
    void RenderStatus();
    void RenderCommandLine();
    void RenderCommandDialog();

    std::vector<Suggestion> BuildSuggestions() const;
    std::vector<std::size_t> VisibleTaskIndices() const;
    int SuggestionStartRow(std::size_t suggestionCount) const;

    static std::string FormatDuration(std::chrono::seconds duration);

    TaskManager taskManager_;
    DisplaySettings displaySettings_;
    CommandProcessor commandProcessor_;
    std::string commandBuffer_;
    std::size_t cursorPosition_{0};
    std::string persistenceStatus_;
    std::vector<std::string> commandHistory_;
    std::optional<std::size_t> historyIndex_;
    std::string commandBeforeHistory_;
    std::size_t selectedSuggestion_{0};
    bool navigatingSuggestions_{false};
    std::optional<CommandDialog> commandDialog_;
    std::chrono::steady_clock::time_point lastAutosave_{};
};
