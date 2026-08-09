# Sentinel

Sentinel is a terminal productivity tracker written in C++20 using ncurses.

## Features

- One-row task list with task ID, state, name, elapsed time, and completion date.
- Live elapsed-time updates while a task is running.
- Command prompt at the bottom of the terminal.
- Interactive fuzzy command palette rendered above the prompt while typing.
- Add, remove, start, stop, complete, and search tasks.
- Ranked, case-insensitive fuzzy search.
- Task commands accept either numeric IDs or approximate task names.
- Keyboard and mouse selection of fuzzy suggestions.

## Interactive command palette

Start typing a command and Sentinel immediately ranks matching commands above the command line:

```text
  start   -  start or resume a task
> stop    -  stop a running task
  search  -  fuzzy-search tasks
------------------------------------------------------------
Tab complete | Up/Down select | click select
> st
```

Controls:

```text
Tab       accept the highlighted suggestion
Up/Down   navigate suggestions
Enter     execute normally, or accept after arrow navigation
Mouse     click a suggestion to accept it
Backspace edit the current command
```

The palette is context-sensitive. After a task-oriented command is completed, suggestions change from command names to task names:

```text
> start opgl

> 0  Develop OpenGL renderer
  3  Refactor OpenGL rendering backend
```

Typing continues to fuzzy-filter and rank those task names. `Tab`, arrow selection, and mouse selection then place the selected task into the command line.

## Commands

```text
add <task name>
remove <task id | fuzzy name>
start <task id | fuzzy name>
stop <task id | fuzzy name>
done <task id | fuzzy name>
search <fuzzy text>
list
help
quit
```

Examples:

```text
add Develop OpenGL renderer
add Study linear algebra
add Refactor rendering system

search opgl rend
start opgl rend
done linear alg
remove refact render
```

Numeric task IDs continue to work:

```text
start 0
stop 0
done 0
```

## Fuzzy matching

Sentinel treats the query as an ordered subsequence of the candidate and ranks matching commands and tasks. The scoring system rewards:

- exact matches,
- prefixes,
- characters appearing consecutively,
- matches beginning at word boundaries,
- matches appearing earlier in the candidate,
- compact matches with fewer gaps.

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

The current version intentionally keeps storage in memory. Logical next steps are JSON persistence, projects/tags, historical sessions, productivity reports, command history, and richer interactive task selection.
