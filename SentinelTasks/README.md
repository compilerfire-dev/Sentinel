# SentinelTasks

SentinelTasks is the tree-oriented terminal companion to Sentinel. The task tree occupies the left side and the selected node's description occupies the right side.

## Tree model

- Folder/category nodes can contain folders or tasks.
- Individual task nodes are leaves.
- Every node has a user-defined ID, name, description, and optional foreground/background color override.
- Task nodes also contain Sentinel-style elapsed-time, running, completed, and completion-time state.
- Folder nodes are structural and do not run timers.
- Tree relationships use terminal-safe ASCII connectors (`+-`, `` `- ``, and `|`).
- Emoji markers are deliberately not part of the task model or command syntax.

## Commands

```text
addFolder <id> <parent|root> <name>
addTask <id> <parent|root> <name>
remove <id>
setDescription
setDescription <id>
setDescription <id> <description>
start <task-id>
stop <task-id>
done <task-id>
showTimes
setJsonFile
setJsonFile <path.json>
defineColor <name> rgb(r,g,b)
color <foreground> bg <background>
color <node-id> <foreground> bg <background>
manualSelect [id]
select <id>
commands
list
quit
```

Names, descriptions, IDs, paths, and custom color names can be enclosed in double quotes when they contain spaces.

## JSON persistence and file selection

SentinelTasks now loads and autosaves `current_data.json`. Its data is stored under the `sentinelTasks` object so unrelated Sentinel/statistics sections in the same JSON file are preserved.

The persisted tree includes node hierarchy, names, descriptions, colors, named colors, elapsed task time, running/completed state, and completion timestamps.

Enter:

```text
setJsonFile
```

to open the GTK native JSON file chooser. Selecting a file saves the current dataset first, loads the chosen file, and changes the active path shown in the SentinelTasks header.

Direct switching remains available:

```text
setJsonFile demo/demo_data.json
setJsonFile "projects/my tasks.json"
```

## Embedded description editor

`setDescription` is integrated directly into the right-side `Description / Timing` pane rather than using the generic argument popup.

```text
setDescription
```

edits the currently selected node. You can also select a node and immediately edit it with:

```text
setDescription opengl
```

The right side becomes an editor while the tree remains visible:

```text
Description / Timing [EDITING]
--------------------------------
ID: opengl
Type: task
Timer: 00:42:17   State: running
Completed: -
Study OpenGL

Read the rendering chapter and implement the examples.
^ cursor is inside this pane
```

Editor controls:

```text
Typing       insert text at the cursor
Enter        insert a newline
Left/Right   move the text cursor
Home/End     move to start/end of the current logical line
Backspace    delete before the cursor
Delete       delete at the cursor when supported by the terminal
Mouse        place the cursor inside the description area
F2           save the description and return to normal mode
Esc          cancel edits and restore the previous description
```

The bottom command line is inactive while the editor is open and shows the current edit mode instead.

For scripts or fast one-line changes, direct command-line assignment remains available:

```text
setDescription opengl "Read the rendering chapter and implement the examples."
```

## Task timers

Task timers behave similarly to Sentinel:

```text
start opengl
stop opengl
start opengl
done opengl
```

`start` begins or resumes elapsed-time measurement, `stop` accumulates the current interval, and `done` stops the timer and records a completion timestamp. Completed task nodes cannot be restarted.

The selected task's timing information is displayed in the right-side `Description / Timing` pane:

```text
ID: opengl
Type: task
Timer: 00:42:17   State: running
Completed: -

Study OpenGL
Read the rendering chapter and implement the examples.
```

The tree also uses `[>]` for a running task and `[x]` for a completed task.

Use:

```text
showTimes
```

to display every task ID with its current timer, state, and completion timestamp.

## Folder and task colors

`color` affects tree rows only. The command prompt, command suggestions, dialogs, status line, and description pane remain in the terminal's normal colors.

```text
color white bg black
color work brightBlue bg black
color opengl green bg black
color "project alpha" rgb(255,210,120) bg rgb(30,30,30)
```

Define reusable colors with:

```text
defineColor focus rgb(60,180,255)
defineColor warning rgb(255,160,40)
defineColor "deep blue" rgb(25,50,120)
```

SentinelTasks also provides the built-in color family used by Sentinel.

## GUI-style argument windows

Other argument-taking commands can still open their ncurses argument windows when entered by name alone. `addFolder` and `addTask` contain ID, parent, and name controls. `remove`, `select`, and `manualSelect` use node drop-lists. `start`, `stop`, and `done` use task-only drop-lists. `defineColor` and `color` provide their color controls.

`setDescription` uses the embedded right-pane editor, while bare `setJsonFile` uses the desktop file chooser.

## Controls

Outside manual selection, the description editor, and argument windows:

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

## Build

On Debian/Ubuntu/Linux Mint:

```bash
sudo apt install libncurses-dev libgtk-3-dev pkg-config cmake g++
cmake -S . -B build
cmake --build build
./build/bin/SentinelTasks
```
