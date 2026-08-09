#pragma once

#include "DisplaySettings.hpp"

#include <chrono>
#include <optional>
#include <string>

class Task {
public:
    using SteadyClock = std::chrono::steady_clock;
    using SystemClock = std::chrono::system_clock;

    Task(std::string id, std::string name);

    const std::string& GetId() const noexcept;
    const std::string& GetName() const noexcept;
    bool IsRunning() const noexcept;
    bool IsCompleted() const noexcept;
    std::chrono::seconds GetElapsedTime() const;
    SystemClock::time_point GetCompletionTime() const noexcept;
    std::string GetCompletionDateString() const;

    void Start();
    void Stop();
    void Complete();
    void Unset();

    void SetColor(RgbColor foreground, RgbColor background);
    void ClearColor();
    bool HasCustomColor() const noexcept;
    const std::optional<RgbColor>& GetForegroundColor() const noexcept;
    const std::optional<RgbColor>& GetBackgroundColor() const noexcept;

    void Restore(
        std::chrono::seconds elapsed,
        bool completed,
        bool running,
        SystemClock::time_point completedAt
    );

private:
    std::string id_;
    std::string name_;
    std::chrono::seconds accumulatedTime_{0};
    bool running_{false};
    bool completed_{false};
    SteadyClock::time_point startedAt_{};
    SystemClock::time_point completedAt_{};
    std::optional<RgbColor> foregroundColor_;
    std::optional<RgbColor> backgroundColor_;
};
