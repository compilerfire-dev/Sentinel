#include "Task.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>
Task::Task(std::string name):name_(std::move(name)){}
const std::string& Task::GetName() const noexcept{return name_;}
bool Task::IsRunning() const noexcept{return running_;}
bool Task::IsCompleted() const noexcept{return completed_;}
void Task::Start(){if(running_||completed_)return; startedAt_=SteadyClock::now(); running_=true;}
void Task::Stop(){if(!running_)return; accumulatedTime_+=std::chrono::duration_cast<std::chrono::seconds>(SteadyClock::now()-startedAt_); running_=false;}
void Task::Complete(){if(completed_)return; Stop(); completed_=true; completedAt_=SystemClock::now();}
std::chrono::seconds Task::GetElapsedTime() const{if(!running_)return accumulatedTime_; return accumulatedTime_+std::chrono::duration_cast<std::chrono::seconds>(SteadyClock::now()-startedAt_);}
std::string Task::GetCompletionDateString() const{if(!completed_)return "-"; const std::time_t raw=SystemClock::to_time_t(completedAt_); std::tm local{};
#ifdef _WIN32
localtime_s(&local,&raw);
#else
localtime_r(&raw,&local);
#endif
std::ostringstream out; out<<std::put_time(&local,"%Y-%m-%d %H:%M"); return out.str();}
