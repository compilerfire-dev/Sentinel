# Sentinel

Sentinel is a terminal productivity tracker written in C++20 using ncurses.

## Features

- One-row task list with task ID, state, name, elapsed time, and completion date.
- Live elapsed-time updates while a task is running.
- Interactive fuzzy command palette above the prompt.
- Ranked fuzzy matching for commands and task names.
- Keyboard and mouse selection of fuzzy suggestions.
- Automatic JSON persistence using the vendored `nlohmann::json` header.
- Runtime switching between JSON data files.
- Configurable foreground/background RGB colors.

## Commands

Type `commands` inside Sentinel to display the complete command list.

```text
add <task name>
remove <task id | fuzzy name>
start <task id | fuzzy name>
stop <task id | fuzzy name>
done <task id | fuzzy name>
search <fuzzy text>
list
commands
setJsonFile <json_file_path.json>
color rgb(r,g,b) bg rgb(r,g,b)
help
quit
```

`rpg(r,g,b)` is also accepted as an alias for `rgb(r,g,b)`.

## JSON persistence

Sentinel starts with:

```text
current_data.json
```

If that file does not exist it is created automatically. Task mutations are saved immediately, running timers are periodically autosaved, and Sentinel saves once more when it exits.

Example JSON structure:

```json
{
    "version": 1,
    "tasks": [
        {
            "name": "Study linear algebra",
            "elapsed_seconds": 1250,
            "running": false,
            "completed": true,
            "completed_at_epoch": 1786267800
        }
    ]
}
```

Running tasks resume from their stored elapsed duration when the file is loaded.

### Switching data files

```text
setJsonFile projects/open_gl.json
```

Before switching, Sentinel saves the currently selected JSON file. It then clears the in-memory task list and selects the requested path.

If the selected file exists, its tasks are loaded. If it does not exist, Sentinel creates a fresh JSON file with an empty task list.

The currently selected JSON file is shown in Sentinel's header.

## Colors

Canonical syntax:

```text
color rgb(255,255,255) bg rgb(0,0,0)
```

For example:

```text
color rgb(0,255,0) bg rgb(0,0,0)
color rgb(255,210,120) bg rgb(20,20,40)
```

Each RGB channel must be from `0` through `255`.

When the terminal supports mutable ncurses colors, Sentinel applies the requested RGB values. On terminals that only support the standard palette, Sentinel chooses the nearest basic terminal color.

## Interactive command palette

Start typing a command and Sentinel ranks matching commands directly above the command line. The new `commands`, `setJsonFile`, and `color` commands are included in this fuzzy palette.

Controls:

```text
Tab       accept the highlighted suggestion
Up/Down   navigate suggestions
Enter     execute normally, or accept after arrow navigation
Mouse     click a suggestion to accept it
Backspace edit the current command
```

After task-oriented commands such as `start`, `stop`, `done`, `remove`, and `search`, the palette switches to fuzzy task-name suggestions.

## Fuzzy matching

Sentinel treats a query as an ordered subsequence and rewards:

- exact matches,
- prefixes,
- consecutive characters,
- word-boundary matches,
- matches occurring earlier in the candidate,
- compact matches with fewer gaps.

Examples:

```text
add Develop OpenGL renderer
add Study linear algebra

start opgl rend
done linear alg
search ogl
```

Numeric task IDs continue to work:

```text
start 0
stop 0
done 0
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

The repository contains `include/nlohmann/json.hpp`, so an additional JSON package is not required.
