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
erase <id>
setDescription
setDescription <id>
setDescription <id> <description>
start <task-id>
stop <task-id>
done <task-id>
unset <task-id>
showTimes
autoSave <duration>
setJsonFile
setJsonFile <path.json>
defineColor <name> rgb(r,g,b)
color <foreground> bg <background>
color <node-id> <foreground> bg <background>
manualSelect
manualSelect <id>
select
select <id>
commands
list
quit
```

`erase` is an explicit alias for `remove`. Erasing a folder recursively removes its descendants as well.

`unset` reverses task completion: `[x]` becomes an idle task again, the completion timestamp is cleared, and accumulated elapsed time is preserved. The task can subsequently be started again with `start`.

Names, descriptions, IDs, paths, and custom color names can be enclosed in double quotes when they contain spaces.

## Selection modes

SentinelTasks intentionally provides two different selection workflows.

### Manual selection

Enter:

```text
manualSelect
```

and SentinelTasks immediately enters the tree selection mode. It does not open an argument popup.

Controls:

```text
Up / Down    previous / next visible node
Left         select parent
Right        select first child
Mouse click  select the clicked tree node
Mouse wheel  scroll overflowing tree rows
Enter        keep selection and leave manual mode
Esc          leave manual mode
```

You can optionally choose the initial node before entering manual mode:

```text
manualSelect opengl
```

### Popup selection

Enter:

```text
select
```

to open the ncurses popup node selector. Choose a node from the drop-list and submit it.

Direct selection remains available:

```text
select opengl
```

so scripts and command-line-oriented workflows do not need to use the popup.

## JSON persistence and file selection

SentinelTasks loads and autosaves `current_data.json`. Its data is stored under the `sentinelTasks` object so unrelated Sentinel/statistics sections in the same JSON file are preserved.

The persisted tree includes node hierarchy, names, descriptions, colors, named colors, elapsed task time, running/completed state, completion timestamps, and autosave interval.

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
unset opengl
start opengl
```

`start` begins or resumes elapsed-time measurement, `stop` accumulates the current interval, and `done` stops the timer and records a completion timestamp. A completed task cannot be restarted until `unset` clears the completed state. `unset` does not reset elapsed time.

The selected task's timing information is displayed in the right-side `Description / Timing` pane:

```text
ID: opengl
Type: task
Timer: 00:42:17   State: running
Completed: -

Study OpenGL
Read the rendering chapter and implement the examples.
```

The tree uses `[>]` for a running task and `[x]` for a completed task.

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

Argument-taking commands can open their ncurses argument windows when entered by name alone. `addFolder` and `addTask` contain ID, parent, and name controls. `remove`, `erase`, and `select` use node drop-lists. `start`, `stop`, `done`, and `unset` use task-only drop-lists. `defineColor` and `color` provide their color controls.

`manualSelect` is deliberately not a popup command: it enters arrow/mouse tree-selection mode immediately. `setDescription` uses the embedded right-pane editor, while bare `setJsonFile` uses the desktop file chooser.

## Controls

Outside manual selection, the description editor, and argument windows:

```text
Tab        accept command suggestion
Down       enter/navigate suggestions
Up         command history
Left/Right edit cursor position
Mouse      position cursor / choose suggestion
Mouse wheel scroll overflowing tree/info rows
Enter      execute command
Backspace  delete before cursor
```

## Build

On Debian/Ubuntu/Linux Mint:

```bash
sudo apt install libncurses-dev libgtk-3-dev pkg-config cmake g++
cmake -S . -B build
cmake --build build
./build/bin/SentinelTasks
```
