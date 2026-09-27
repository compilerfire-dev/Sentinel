
#include <sentinel/cmdargs.h>
#include <string>
#include <string_view>

void cmdArgs::Process(int argc, char* argv[]) {
    verboseOutput = false;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);

        if (arg == "--verbose") {
            verboseOutput = true;
        } else if (arg.rfind("--verbose=", 0) == 0) {
            std::string_view value = arg.substr(10);

            if (value == "1" || value == "true") {
                verboseOutput = true;
            } else if (value == "0" || value == "false") {
                verboseOutput = false;
            }
        }
    }
}

namespace sentinel {
    cmdArgs cmdargs;
}

void sentinel_process_cmd_arguments(int argc, char* argv[]) {
    sentinel::cmdargs.Process(argc, argv);
}