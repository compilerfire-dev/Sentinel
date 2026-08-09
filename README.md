# Sentinel

Sentinel is a terminal productivity tracker written in C++20 using ncurses.

## Features

- One-row task list with user-defined task ID, state, name, elapsed time, and completion date.
- Live elapsed-time updates while a task is running.
- Interactive fuzzy command palette above the prompt.
- Ranked fuzzy matching for commands, task IDs, and task names.
- Keyboard and mouse selection of fuzzy suggestions.
- Command history recall with the Up/Down arrow keys.
- Automatic JSON persistence using the vendored `nlohmann::json` header.
- Runtime switching between JSON data files.
- Default task-row RGB colors plus per-task color overrides.

## Commands

Type `commands` inside Sentinel to display the complete command list.

```text
add <id> <name>
add "<id>" "<name>"
remove <id | fuzzy name>
start <id | fuzzy name>
stop <id | fuzzy name>
done <id | fuzzy name>
search <fuzzy text>
list
commands
setJsonFile <json_file_path.json>
color rgb(r,g,b) bg rgb(r,g,b)
color <id> rgb(r,g,b) bg rgb(r,g,b)
help
quit
```

`rpg(r,g,b)` is also accepted as an alias for `rgb(r,g,b)`.

## User-defined task IDs and quoting

The number previously shown as the vector position is now replaced by a persistent ID supplied by you when the task is created.

Examples:

```text
add math Study linear algebra
add ogl "Develop OpenGL renderer"
add "linear algebra" "Study Linear Algebra - Chapter 1"
add "project alpha" "Refactor rendering backend"
```

Both the ID and the task name may be enclosed in double quotes. Quotes are required when the ID contains spaces. The name may either be quoted or consume the remainder of the `add` command.

Tasks are rendered approximately as:

```text
math [ ] Study linear algebra - 00:00:00 - -
ogl  [>] Develop OpenGL renderer - 00:12:43 - -
```

Task commands prefer an exact ID match, but fuzzy task-name matching remains available:

```text
start math
start "linear algebra"
done ogl
remove "project alpha"
```

## JSON persistence

Sentinel starts with:

```text
current_data.json
```

If that file does not exist it is created automatically. Task mutations are saved immediately, running timers are periodically autosaved, and Sentinel saves once more when it exits.

The JSON format now stores the user-defined ID and optional per-task colors:

```json
{
    "version": 2,
    "tasks": [
        {
            "id": "math",
            "name": "Study linear algebra",
            "elapsed_seconds": 1250,
            "running": false,
            "completed": true,
            "completed_at_epoch": 1786267800,
            "color": {
                "foreground": {"red": 0, "green": 255, "blue": 0},
                "background": {"red": 0, "green": 0, "blue": 0}
            }
        }
    ]
}
```

Older Sentinel JSON files without an `id` field are still loadable; Sentinel assigns IDs based on their previous list positions during migration.

### Switching data files

```text
setJsonFile projects/open_gl.json
setJsonFile "projects/my data.json"
```

Before switching, Sentinel saves the currently selected JSON file. It then clears the in-memory task list and selects the requested path.

If the selected file exists, its tasks are loaded. If it does not exist, Sentinel creates a fresh JSON file with an empty task list.

The currently selected JSON file is shown in Sentinel's header.

## Task colors

The `color` command affects task rows only. The header, fuzzy palette, status text, and command prompt remain in the terminal's normal colors.

Set the default color for all tasks without an override:

```text
color rgb(255,255,255) bg rgb(0,0,0)
```

Set a color for a particular task ID:

```text
color math rgb(0,255,0) bg rgb(0,0,0)
color "linear algebra" rgb(255,210,120) bg rgb(20,20,40)
```

Each RGB channel must be from `0` through `255`. Per-task colors are saved to the active JSON file.

When the terminal supports mutable ncurses colors and has enough color slots, Sentinel uses the requested RGB values for visible tasks. On more limited terminals it chooses the nearest standard terminal color.

## Interactive command palette and history

Start typing a command and Sentinel ranks matching commands directly above the command line. Task-oriented commands display task suggestions as:

```text
math  -  Study linear algebra
ogl   -  Develop OpenGL renderer
```

Choosing a suggestion inserts its task ID, quoting it automatically when the ID contains spaces.

Controls:

```text
Tab       accept the highlighted suggestion
Down      begin navigating fuzzy suggestions / move downward
Up        recall the previous command when not navigating suggestions
Up/Down   navigate suggestions once suggestion navigation is active
Up/Down   move backward/forward through command history while browsing history
Enter     execute normally, or accept after suggestion navigation
Mouse     click a suggestion to accept it
Backspace edit the current command
```

## Fuzzy matching

Sentinel fuzzy-searches both task IDs and task names. Exact task IDs receive priority when executing `start`, `stop`, `done`, and `remove`.

Examples:

```text
add ogl "Develop OpenGL renderer"
add math "Study linear algebra"

start ogl
done math
search opgl rend
search linear alg
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
