#pragma once
#include <chrono>
#include <string>
class Task { public: using SteadyClock=std::chrono::steady_clock; using SystemClock=std::chrono::system_clock; explicit Task(std::string name); const std::string& GetName() const noexcept; bool IsRunning() const noexcept; bool IsCompleted() const noexcept; std::chrono::seconds GetElapsedTime() const; std::string GetCompletionDateString() const; void Start(); void Stop(); void Complete(); private: std::string name_; std::chrono::seconds accumulatedTime_{0}; bool running_{false}; bool completed_{false}; SteadyClock::time_point startedAt_{}; SystemClock::time_point completedAt_{}; };
