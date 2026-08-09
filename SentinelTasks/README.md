# SentinelTasks

SentinelTasks is the tree-oriented terminal companion to Sentinel. It is designed for a wide, full-screen terminal: the task tree occupies the left side and the selected node's description occupies the right side.

## Tree model

- Folder/category nodes can contain folders or tasks.
- Individual task nodes are leaves.
- Every node has a user-defined ID, name, emoji marker, and description.
- Tree relationships are rendered with `├─`, `└─`, and `│` connectors.
- The supplied square markers are intended naturally for task folders and the circle markers for individual tasks, but every allowed marker can be assigned to either node type.

## Commands

```text
addFolder <id> <parent|root> <emoji> <name>
addTask <id> <parent|root> <emoji> <name>
remove <id>
setDescription <id> <description>
setEmoji <id> <emoji>
manualSelect [id]
select <id>
emojis
commands
list
quit
```

Names, descriptions, and IDs can be enclosed in double quotes when they contain spaces.

Examples:

```text
addFolder work root 🟥 "Programming"
addFolder graphics work 🟦 "Graphics programming"
addTask opengl graphics 🔵 "Study OpenGL"
setDescription opengl "Read the rendering chapter and implement the examples."
manualSelect opengl
```

## GUI-style command argument windows

Commands that require arguments can be entered by name alone. Instead of producing a usage error, SentinelTasks opens a centered ncurses argument window with text-input fields and drop-lists.

For example:

```text
> addTask
```

opens fields equivalent to:

```text
ID:      [ text input                         ]
Parent:  [ drop-list: root / existing folders]
Emoji:   [ drop-list: allowed emoji markers  ]
Name:    [ text input                         ]

                 [ Submit ]   [ Cancel ]
```

The forms are command-aware:

- `addFolder` / `addTask`: ID text input, parent-folder drop-list, emoji drop-list, name text input.
- `remove`: existing-node drop-list.
- `setDescription`: existing-node drop-list and description text input.
- `setEmoji`: existing-node drop-list and emoji drop-list.
- `select`: existing-node drop-list.
- `manualSelect`: existing-node drop-list, followed by normal manual-selection mode after submission.

Drop-list values are rebuilt from the current tree when the window opens. Parent selection includes `root` plus existing folder IDs; node selections contain currently existing nodes; emoji selection uses the complete allowed marker pool.

Inline CLI arguments still work exactly as before. For example, both of these are valid:

```text
addTask
addTask opengl graphics 🔵 "Study OpenGL"
```

### Argument-window controls

```text
Tab / Shift-Tab   move between controls
Left/Right        edit cursor in text fields / move between buttons
Up/Down           choose items in a drop-list
Enter / Space     open or accept a drop-list; activate buttons
Enter             on a text field advances to the next control
F2                submit the complete form immediately
Esc               close an open drop-list, or cancel the argument window
Mouse             focus fields, open/select drop-list values, submit/cancel
Backspace         edit the focused text field
Home / End        move inside the focused text field
```

The form submits by rebuilding the corresponding textual command and sending it through the same command parser used by normal CLI input.

## Manual selection

Run:

```text
manualSelect
```

This now opens the node-selection argument window first. Select a starting node and submit it, then use:

```text
Up/Down    previous/next visible node
Left       select parent
Right      select first child
Mouse      click a visible tree row
Enter      keep selection and leave manualSelect
Esc        leave manualSelect
```

The right pane updates immediately as the selected node changes.

## Emoji markers

SentinelTasks accepts this marker set:

```text
🔘 🔴 🟠 🟡 🟢 🔵 🟣 ⚫️ ⚪️ 🟤
🔺 🔻 🔸 🔹 🔶 🔷
🔳 🔲 ▪️ ▫️ ◾️ ◽️ ◼️ ◻️
🟥 🟧 🟨 🟩 🟦 🟪 ⬛️ ⬜️ 🟫
```

Use `emojis` inside the application to display them.

## Command-line controls

Outside `manualSelect` and the argument window:

```text
Tab        accept command suggestion
Down       enter/navigate command suggestions
Up         command history
Left/Right edit cursor position
Mouse      position cursor / choose suggestion
Enter      execute command
Backspace  delete before cursor
```

UTF-8 input is enabled before ncurses starts so the markers can be entered directly on terminals with emoji-capable fonts.

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build
./build/bin/SentinelTasks
```
