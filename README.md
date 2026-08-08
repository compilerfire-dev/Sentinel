# Sentinel

Sentinel is a terminal productivity tracker written in C++20 using ncurses.

## Features

- One-row task list with task ID, state, name, elapsed time, and completion date.
- Live elapsed-time updates while a task is running.
- Command prompt at the bottom of the terminal.
- Add, remove, start, stop, complete, and search tasks.
- Case-insensitive substring search.

## Commands

```text
add <task name>
remove <task id>
start <task id>
stop <task id>
done <task id>
search <text>
list
help
quit
```

Task states:

```text
[ ] idle
[>] running
[x] completed
```

## Build

On Debian/Ubuntu/Linux Mint:

```bash
sudo apt install libncurses-dev cmake g++
cmake -S . -B build
cmake --build build
./build/Sentinel
```

## Next milestones

The initial version intentionally keeps storage in memory. Logical next steps are scored fuzzy matching, JSON persistence, projects/tags, historical sessions, and productivity reports.
