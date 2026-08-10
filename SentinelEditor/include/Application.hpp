#pragma once

#include "EditorBuffer.hpp"
#include "TypingMetrics.hpp"

#include <cstddef>
#include <string>

class Application {
public:
    int Run(int argc, char** argv);

private:
    enum class Mode {
        Normal,
        Insert,
        Command,
        Search
    };

    void HandleInput(int key);
    void HandleNormalInput(int key);
    void HandleInsertInput(int key);
    void HandleCommandInput(int key);
    void HandleSearchInput(int key);
    void HandleMouse();

    void EnterNormalMode();
    void EnterInsertMode();
    void EnterCommandMode();
    void EnterSearchMode();

    void ExecuteCommand(const std::string& command);
    bool LoadFile(const std::string& path, bool force);
    bool SaveFile(const std::string& path = {});

    void MoveHorizontal(int delta);
    void MoveVertical(int delta);
    void MoveWordForward();
    void MoveWordBackward();
    void MovePage(int direction);
    void ClampCursor();
    void EnsureCursorVisible();

    bool FindNext(const std::string& query, bool wrap);

    void Render();
    void RenderBuffer(int height, int width);
    void RenderStatusLine(int row, int width);
    void RenderCommandLine(int row, int width);

    std::size_t GutterWidth() const;
    std::size_t TextRows() const;
    std::size_t TextColumns() const;
    std::string ModeName() const;

    static std::string Trim(std::string value);
    static std::string Unquote(std::string value);
    static std::string ExpandTabs(const std::string& value, std::size_t tabWidth = 4);

    EditorBuffer buffer_;
    TypingMetrics typingMetrics_;
    Mode mode_{Mode::Normal};

    std::size_t cursorRow_{0};
    std::size_t cursorColumn_{0};
    std::size_t preferredColumn_{0};
    std::size_t topLine_{0};
    std::size_t leftColumn_{0};

    bool running_{true};
    bool pendingDelete_{false};
    bool pendingGoto_{false};

    std::string commandBuffer_;
    std::string searchBuffer_;
    std::string lastSearch_;
    std::string status_;
};
