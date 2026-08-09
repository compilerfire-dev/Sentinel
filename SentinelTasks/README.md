# SentinelTasks

SentinelTasks is the tree-oriented terminal companion to Sentinel. The task tree occupies the left side and the selected node's description occupies the right side.

## Tree model

- Folder/category nodes can contain folders or tasks.
- Individual task nodes are leaves.
- Every node has a user-defined ID, name, description, and optional foreground/background color override.
- Tree relationships use terminal-safe ASCII connectors (`+-`, `` `- ``, and `|`).
- Emoji markers are deliberately not part of the task model or command syntax.

## Commands

```text
addFolder <id> <parent|root> <name>
addTask <id> <parent|root> <name>
remove <id>
setDescription <id> <description>
defineColor <name> rgb(r,g,b)
color <foreground> bg <background>
color <node-id> <foreground> bg <background>
manualSelect [id]
select <id>
commands
list
quit
```

Names, descriptions, IDs, and custom color names can be enclosed in double quotes when they contain spaces.

Examples:

```text
addFolder work root "Programming"
addFolder graphics work "Graphics programming"
addTask opengl graphics "Study OpenGL"
setDescription opengl "Read the rendering chapter and implement the examples."
manualSelect opengl
```

## Folder and task colors

`color` affects tree rows only. The command prompt, command suggestions, dialogs, status line, and description pane remain in the terminal's normal colors.

Set the default tree-row foreground/background:

```text
color white bg black
color brightGreen bg black
color rgb(230,230,230) bg rgb(20,20,20)
```

Override one folder or task by its ID:

```text
color work brightBlue bg black
color opengl green bg black
color "project alpha" rgb(255,210,120) bg rgb(30,30,30)
```

SentinelTasks provides the same built-in color family as Sentinel:

```text
black red green yellow blue magenta cyan white
brightBlack gray grey
brightRed brightGreen brightYellow brightBlue
brightMagenta brightCyan brightWhite
```

Capitalization and separators in built-in names are normalized, so forms such as `brightBlue`, `bright-blue`, and `bright_blue` resolve equivalently.

### Custom named colors

Define a reusable color with:

```text
defineColor focus rgb(60,180,255)
defineColor warning rgb(255,160,40)
defineColor "deep blue" rgb(25,50,120)
```

Then use it anywhere a color value is accepted:

```text
color work focus bg black
color opengl warning bg black
color "project alpha" "deep blue" bg white
```

Each RGB channel must be between 0 and 255. RGB values are mapped to the nearest color exposed by the current ncurses terminal palette, matching Sentinel's portable color behavior.

## GUI-style argument windows

Entering an argument-taking command by itself opens its ncurses argument window. `addFolder` and `addTask` contain ID, parent, and name controls; there is no emoji field.

```text
> addTask

ID:      [ text input                          ]
Parent:  [ root / existing folders          v ]
Name:    [ text input                          ]

                 [ Submit ]   [ Cancel ]
```

`remove`, `select`, and `manualSelect` use an existing-node drop-list. `setDescription` uses a node drop-list and description text input.

`defineColor` opens a color-name field plus an RGB text field. `color` opens a target drop-list (`default` plus existing node IDs) and foreground/background drop-lists populated from the built-in and currently defined named colors.

Inline CLI arguments remain supported:

```text
addTask opengl graphics "Study OpenGL"
defineColor focus rgb(60,180,255)
color opengl focus bg black
```

## Controls

Outside manual selection and argument windows:

```text
Tab        accept command suggestion
Down       enter/navigate suggestions
Up         command history
Left/Right edit cursor position
Mouse      position cursor / choose suggestion
Enter      execute command
Backspace  delete before cursor
```

Manual selection uses Up/Down for nodes, Left for parent, Right for first child, mouse click for selection, and Enter/Esc to finish.

Argument windows use Tab to move between controls, Up/Down for drop-list choices, Enter to advance/activate, F2 to submit, Esc to cancel, and Backspace/Left/Right for text editing.

## Build

```bash
cmake -S . -B build
cmake --build build
./build/bin/SentinelTasks
```
