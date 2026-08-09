# SentinelTasks

SentinelTasks is the tree-oriented terminal companion to Sentinel. The task tree occupies the left side and the selected node's description occupies the right side.

## Tree model

- Folder/category nodes can contain folders or tasks.
- Individual task nodes are leaves.
- Every node has a user-defined ID, name, and description.
- Tree relationships use terminal-safe ASCII connectors (`+-`, `` `- ``, and `|`).
- Emoji markers are deliberately not part of the task model or command syntax.

## Commands

```text
addFolder <id> <parent|root> <name>
addTask <id> <parent|root> <name>
remove <id>
setDescription <id> <description>
manualSelect [id]
select <id>
commands
list
quit
```

Names, descriptions, and IDs can be enclosed in double quotes when they contain spaces.

Examples:

```text
addFolder work root "Programming"
addFolder graphics work "Graphics programming"
addTask opengl graphics "Study OpenGL"
setDescription opengl "Read the rendering chapter and implement the examples."
manualSelect opengl
```

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

Inline CLI arguments remain supported:

```text
addTask opengl graphics "Study OpenGL"
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
