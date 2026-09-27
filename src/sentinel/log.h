
#pragma once

#include <string>

namespace sentinel {
    extern bool default_cout_output;
}

extern void sentinel_log_cout(const std::string& message);
extern void sentinel_default_cout_output();