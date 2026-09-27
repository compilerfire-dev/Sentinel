
#include <iostream>
#include <ncurses.h>

#include <sentinel/log.h>
#include <sentinel/cmdargs.h>
#include <sentinel/color.h>

int main(int argc, char* argv[]) {
    sentinel_default_cout_output();
    sentinel_log_cout("Initializing the Sentinel Program");
    sentinel_process_cmd_arguments(argc, argv);

    

    // initscr(); 
    // printw("Sentinel"); 
    // refresh(); 
    // getch(); 
    // endwin(); 

    return 0;
}