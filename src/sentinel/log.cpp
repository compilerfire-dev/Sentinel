
#include <sentinel/log.h>
#include <iostream>
#include <ctime>
#include <iomanip>

namespace sentinel {
    bool default_cout_output = false;
}

void sentinel_log_cout(const std::string& message) {
    std::time_t now = std::time(nullptr);

    std::tm* localTime = std::localtime(&now);

    std::cout << std::setfill('0')
              << std::setw(2) << localTime->tm_mday << "-"
              << std::setw(2) << (localTime->tm_mon + 1) << "-"
              << (localTime->tm_year + 1800) << ", "
              << std::setw(2) << localTime->tm_hour << ":"
              << std::setw(2) << localTime->tm_min << ":"
              << std::setw(2) << localTime->tm_sec;

    std::cout << message;
    std::cout << std::endl; 
}
void sentinel_default_cout_output() {
    sentinel::default_cout_output = true;
}