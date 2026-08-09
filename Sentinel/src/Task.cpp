#include "Task.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {

std::string FormatTimePoint(Task::SystemClock::time_point value) {
    if (value.time_since_epoch() == Task::SystemClock::duration::zero()) return "-";
    const std::time_t raw = Task::SystemClock::to_time_t(value);
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

} // namespace

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

    SentinelShared::TimeFragment fragment;
    fragment.startedAt = SystemClock::now();
    timeFragments_.push_back(fragment);
}

void Task::Stop() {
    if (!running_) return;

    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        SteadyClock::now() - startedAt_
    );
    accumulatedTime_ += elapsed;
    running_ = false;

    if (!timeFragments_.empty() && timeFragments_.back().IsOpen()) {
        timeFragments_.back().endedAt = SystemClock::now();
        timeFragments_.back().duration = elapsed;
    }
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
    return accumulatedTime_ + std::chrono::duration_cast<std::chrono::seconds>(
        SteadyClock::now() - startedAt_
    );
}

Task::SystemClock::time_point Task::GetCreatedTime() const noexcept {
    return createdAt_;
}

Task::SystemClock::time_point Task::GetCompletionTime() const noexcept {
    return completedAt_;
}

std::string Task::GetCreatedDateString() const {
    return FormatTimePoint(createdAt_);
}

std::string Task::GetCompletionDateString() const {
    return completed_ ? FormatTimePoint(completedAt_) : "-";
}

std::vector<SentinelShared::TimeFragment> Task::GetTimeFragments() const {
    auto fragments = timeFragments_;
    if (running_ && !fragments.empty() && fragments.back().IsOpen()) {
        fragments.back().duration = std::chrono::duration_cast<std::chrono::seconds>(
            SteadyClock::now() - startedAt_
        );
    }
    return fragments;
}

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

const std::optional<RgbColor>& Task::GetForegroundColor() const noexcept {
    return foregroundColor_;
}

const std::optional<RgbColor>& Task::GetBackgroundColor() const noexcept {
    return backgroundColor_;
}

void Task::Restore(
    std::chrono::seconds elapsed,
    bool completed,
    bool running,
    SystemClock::time_point createdAt,
    SystemClock::time_point completedAt,
    std::vector<SentinelShared::TimeFragment> fragments
) {
    accumulatedTime_ = elapsed;
    completed_ = completed;
    createdAt_ = createdAt;
    completedAt_ = completedAt;
    running_ = false;
    timeFragments_ = std::move(fragments);

    // A saved process may have had an open fragment. The saved duration is the
    // last known measured portion. Close that historical fragment at its last
    // measured point so application downtime is never counted as work.
    for (auto& fragment : timeFragments_) {
        if (fragment.IsOpen()) {
            fragment.endedAt = fragment.startedAt + fragment.duration;
        }
    }

    if (running && !completed_) Start();
}
