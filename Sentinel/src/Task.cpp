#include "Task.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

Task::Task(std::string id, std::string name)
    : id_(std::move(id)), name_(std::move(name)) {}

const std::string& Task::GetId() const noexcept { return id_; }
const std::string& Task::GetName() const noexcept { return name_; }
bool Task::IsRunning() const noexcept { return running_; }
bool Task::IsCompleted() const noexcept { return completed_; }

void Task::Start() {
    if (running_ || completed_) return;
    startedAt_ = SteadyClock::now();
    running_ = true;
}

void Task::Stop() {
    if (!running_) return;
    accumulatedTime_ += std::chrono::duration_cast<std::chrono::seconds>(SteadyClock::now() - startedAt_);
    running_ = false;
}

void Task::Complete() {
    if (completed_) return;
    Stop();
    completed_ = true;
    completedAt_ = SystemClock::now();
}

void Task::Unset() {
    if (!completed_) return;
    completed_ = false;
    running_ = false;
    completedAt_ = SystemClock::time_point{};
}

std::chrono::seconds Task::GetElapsedTime() const {
    if (!running_) return accumulatedTime_;
    return accumulatedTime_ + std::chrono::duration_cast<std::chrono::seconds>(SteadyClock::now() - startedAt_);
}

Task::SystemClock::time_point Task::GetCompletionTime() const noexcept { return completedAt_; }

void Task::SetColor(RgbColor foreground, RgbColor background) {
    foregroundColor_ = foreground;
    backgroundColor_ = background;
}

void Task::ClearColor() {
    foregroundColor_.reset();
    backgroundColor_.reset();
}

bool Task::HasCustomColor() const noexcept {
    return foregroundColor_.has_value() && backgroundColor_.has_value();
}

const std::optional<RgbColor>& Task::GetForegroundColor() const noexcept { return foregroundColor_; }
const std::optional<RgbColor>& Task::GetBackgroundColor() const noexcept { return backgroundColor_; }

void Task::Restore(std::chrono::seconds elapsed, bool completed, bool running, SystemClock::time_point completedAt) {
    accumulatedTime_ = elapsed;
    completed_ = completed;
    completedAt_ = completedAt;
    running_ = false;
    if (running && !completed_) Start();
}

std::string Task::GetCompletionDateString() const {
    if (!completed_) return "-";
    const std::time_t raw = SystemClock::to_time_t(completedAt_);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &raw);
#else
    localtime_r(&raw, &local);
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d %H:%M");
    return out.str();
}
