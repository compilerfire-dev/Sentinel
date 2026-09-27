
#pragma once

#include <map>
#include <string>

class cmdArgs {
public:

    cmdArgs() = default;

    void Process(int argc, char* argv[]);

    inline bool GetVerboseOutputFlag() const {
        return verboseOutput;
    }
protected:
    bool verboseOutput = false;
};

namespace sentinel {
    extern cmdArgs cmdargs;
}

extern void sentinel_process_cmd_arguments(int argc, char* argv[]);