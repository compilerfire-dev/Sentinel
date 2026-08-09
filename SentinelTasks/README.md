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

## Manual selection

Run:

```text
manualSelect
```

Then use:

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

Outside `manualSelect`:

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
