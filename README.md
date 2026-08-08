# Sentinel

Sentinel is a terminal productivity tracker written in C++20 using ncurses.

## Features

- One-row task list with task ID, state, name, elapsed time, and completion date.
- Live elapsed-time updates while a task is running.
- Command prompt at the bottom of the terminal.
- Add, remove, start, stop, complete, and search tasks.
- Ranked, case-insensitive fuzzy search.
- Task commands accept either numeric IDs or approximate task names.

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

Sentinel treats the query as an ordered subsequence of the task name and ranks every matching task. The scoring system rewards:

- exact matches,
- prefixes,
- characters appearing consecutively,
- matches beginning at word boundaries,
- matches appearing earlier in the task name,
- compact matches with fewer gaps.

For commands such as `start`, `stop`, `done`, and `remove`, the highest-ranked fuzzy result is selected. The `search` command displays all matches in ranked order.

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

The current version intentionally keeps storage in memory. Logical next steps are JSON persistence, projects/tags, historical sessions, productivity reports, and interactive fuzzy-selection when several task names score similarly.
