#include "TaskManager.hpp"
#include <algorithm>
#include <cctype>
#include <utility>
namespace { std::string Lower(std::string v){std::transform(v.begin(),v.end(),v.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));}); return v;} }
Task& TaskManager::AddTask(std::string name){tasks_.emplace_back(std::move(name)); return tasks_.back();}
bool TaskManager::RemoveTask(std::size_t index){if(index>=tasks_.size())return false; tasks_.erase(tasks_.begin()+static_cast<std::ptrdiff_t>(index)); return true;}
Task* TaskManager::GetTask(std::size_t index){return index<tasks_.size()?&tasks_[index]:nullptr;}
const Task* TaskManager::GetTask(std::size_t index) const{return index<tasks_.size()?&tasks_[index]:nullptr;}
std::vector<std::size_t> TaskManager::Search(const std::string& query) const{std::vector<std::size_t> out; const auto q=Lower(query); for(std::size_t i=0;i<tasks_.size();++i)if(Lower(tasks_[i].GetName()).find(q)!=std::string::npos)out.push_back(i); return out;}
const std::vector<Task>& TaskManager::GetTasks() const noexcept{return tasks_;}
