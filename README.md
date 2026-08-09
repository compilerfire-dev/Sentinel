# Sentinel

Sentinel is a terminal productivity tracker written in C++20 using ncurses.

## Features

- One-row task list with user-defined task ID, state, name, elapsed time, and completion date.
- Live elapsed-time updates while a task is running.
- Interactive fuzzy command palette above the prompt.
- Ranked fuzzy matching for commands, task IDs, and task names.
- Keyboard and mouse selection of fuzzy suggestions.
- Command history recall with the Up/Down arrow keys.
- Left/Right command-line cursor editing and mouse cursor placement.
- Automatic JSON persistence using the vendored `nlohmann::json` header.
- Runtime switching between JSON data files.
- Default task-row colors plus per-task color overrides.
- Built-in named colors and user-defined named RGB colors.

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
defineColor <color-id> rgb(r,g,b)
color <foreground> bg <background>
color <task-id> <foreground> bg <background>
help
quit
```

`rpg(r,g,b)` is also accepted as an alias for `rgb(r,g,b)`.

## User-defined task IDs and quoting

The number previously shown as the vector position is now replaced by a persistent ID supplied when the task is created.

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

The JSON format stores tasks, per-task colors, and custom named colors:

```json
{
    "version": 3,
    "defined_colors": {
        "focus": {"red": 60, "green": 180, "blue": 255},
        "warning": {"red": 255, "green": 160, "blue": 40}
    },
    "tasks": [
        {
            "id": "math",
            "name": "Study linear algebra",
            "elapsed_seconds": 1250,
            "running": false,
            "completed": true,
            "completed_at_epoch": 1786267800,
            "color": {
                "foreground": {"red": 60, "green": 180, "blue": 255},
                "background": {"red": 0, "green": 0, "blue": 0}
            }
        }
    ]
}
```

Older Sentinel JSON files remain loadable. Files without task IDs receive IDs based on their previous list positions, and files without `defined_colors` simply start with an empty custom-color registry.

### Switching data files

```text
setJsonFile projects/open_gl.json
setJsonFile "projects/my data.json"
```

Before switching, Sentinel saves the currently selected JSON file. It then loads the requested dataset. Named color definitions belong to the selected JSON dataset and switch with it.

## Task colors

The `color` command affects task rows only. The header, fuzzy palette, status text, and command prompt remain in the terminal's normal colors.

### Built-in color names

Sentinel includes these named colors:

```text
black
red
green
yellow
blue
magenta
cyan
white
brightBlack / gray / grey
brightRed
brightGreen
brightYellow
brightBlue
brightMagenta
brightCyan
brightWhite
```

Name matching ignores capitalization and separators such as `_` or `-`, so `bright_red`, `BrightRed`, and `bright-red` resolve to the same built-in color.

Use named colors directly:

```text
color white bg black
color math brightGreen bg black
color "linear algebra" cyan bg blue
```

Direct RGB remains available:

```text
color rgb(255,255,255) bg rgb(0,0,0)
color math rgb(0,255,0) bg rgb(0,0,0)
```

### Defining custom named colors

Create or replace a named color with:

```text
defineColor focus rgb(60,180,255)
defineColor warning rgb(255,160,40)
defineColor "deep blue" rgb(25,50,120)
```

Then use those names anywhere a color value is accepted:

```text
color focus bg black
color math warning bg black
color "linear algebra" "deep blue" bg white
```

Custom names are stored in the active JSON file. A later `defineColor` with the same ID replaces the previous RGB value.

Each RGB channel must be from `0` through `255`. Per-task colors are stored as resolved RGB values, so a task keeps its assigned appearance even if the named color definition is later changed.

When the terminal supports mutable ncurses colors and has enough color slots, Sentinel uses the requested RGB values for visible tasks. On more limited terminals it chooses the nearest standard terminal color.

## Interactive command palette and history

Start typing a command and Sentinel ranks matching commands directly above the command line. `defineColor` is included in the fuzzy command palette.

Task-oriented commands display task suggestions as:

```text
math  -  Study linear algebra
ogl   -  Develop OpenGL renderer
```

Choosing a suggestion inserts its task ID, quoting it automatically when the ID contains spaces.

Controls:

```text
Tab        accept the highlighted suggestion
Down       begin navigating fuzzy suggestions / move downward
Up         recall the previous command when not navigating suggestions
Up/Down    navigate suggestions once suggestion navigation is active
Left/Right move the command-line cursor
Mouse      click command line to position cursor; click suggestion to accept it
Enter      execute normally, or accept after suggestion navigation
Backspace  delete the character before the cursor
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
