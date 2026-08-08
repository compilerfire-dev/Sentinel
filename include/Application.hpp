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
    void HandleInput();
    void Render();
    void RenderHeader();
    void RenderTasks();
    void RenderStatus();
    void RenderCommandLine();

    std::vector<std::size_t> VisibleTaskIndices() const;
    static std::string FormatDuration(std::chrono::seconds duration);

    TaskManager taskManager_;
    CommandProcessor commandProcessor_;
    std::string commandBuffer_;
};
