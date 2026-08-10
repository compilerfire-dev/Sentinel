# SentinelTasks

SentinelTasks is the hierarchical ncurses companion to Sentinel. Folders provide tree structure, while every task node refers to one globally shared task identity used by Sentinel, SentinelTasks, and SentinelStats.

## Shared task model

Tasks exist exactly once under `sharedTasks.tasks`. SentinelTasks stores tree-specific metadata around the same global task ID: parent placement and description. Task name, timers, completion state, creation time, colors, and time fragments remain canonical shared-task data.

A task created in Sentinel appears at SentinelTasks root when the dataset is next loaded if it has no placement. A task created in SentinelTasks appears in Sentinel on reload.

## Tree model

- Folders may contain folders and tasks.
- Tasks are leaves and use their global shared-task IDs.
- Folder IDs cannot collide with global task IDs.
- Folder collapse state is persisted in `sentinelTasks.nodes`.
- `[+]` is a collapsed folder with children.
- `[-]` is an expanded folder with children.
- `[F]` is an empty folder.
- `[T]`, `[>]`, and `[x]` are idle, running, and completed tasks.

Collapse affects only the visible UI projection. Hidden descendants remain in the full tree and are always persisted.

## Commands

```text
addFolder <id> <parent|root> <name>
addTask <id> <parent|root> <name>
move <id> <parent|root>
rename <id> <new name>
collapse <folder-id>
expand <folder-id>
toggle <folder-id>
remove <id>
erase <id>
clear
clear <id>
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

### Moving nodes

```text
move opengl graphics
move graphics root
```

The destination must be `root` or a folder. SentinelTasks rejects self-parenting and moves that would create a cycle by putting a folder inside one of its descendants. Moving into a collapsed folder expands the destination so the moved node remains visible.

### Renaming nodes

```text
rename opengl "Modern OpenGL"
rename graphics "Graphics Programming"
```

`rename` changes the display name, never the ID. Renaming a task therefore updates the canonical shared task name while preserving its global identity and history. Renaming a folder is tree-local.

### Collapsing folders

```text
collapse graphics
expand graphics
toggle graphics
```

The collapsed state is persisted with the selected JSON dataset. Direct selection or movement automatically expands ancestors when necessary to reveal the selected node.

All five commands also support the ncurses argument popup when entered without arguments.

## Selection

`manualSelect` enters direct tree selection mode:

```text
Up / Down    previous / next visible node
Left         parent
Right        expand folder and select first child
Mouse click  select node
Mouse wheel  scroll
Enter / Esc  leave manual mode
```

`select` opens the popup node selector; `select <id>` selects directly and reveals hidden ancestors.

## Description editor

`setDescription` edits the selected node in the right-hand pane. `F2` saves and `Esc` cancels. Direct assignment remains available:

```text
setDescription opengl "Read and implement the rendering examples."
```

## Task timing

Every continuous `start -> stop` or `start -> done` session is recorded as one `time_fragments` entry in the shared task. `unset` clears completion state without deleting accumulated time or fragments.

## Persistence

SentinelTasks defaults to `current_data.json` and uses the shared locked + atomic datastore. `sentinelTasks` schema version 4 stores folder hierarchy, descriptions, folder collapse state, display configuration, named colors, and autosave configuration. `sharedTasks.tasks` owns canonical task state.

Use:

```text
setJsonFile
```

to open the native file chooser, or:

```text
setJsonFile demo/demo_data.json
```

to switch directly.

## Build and tests

```bash
sudo apt install libncurses-dev libgtk-3-dev pkg-config cmake g++
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/bin/SentinelTasks
```

`SentinelTasks.TreeModel` tests renaming, moving/reparenting, cycle rejection, collapse visibility, ancestor expansion, and full-tree preservation.
