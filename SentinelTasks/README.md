# SentinelTasks

SentinelTasks is the hierarchical ncurses companion to Sentinel. Folders provide tree structure, while every task node refers to one globally shared task identity used by both terminal applications and SentinelStats.

## Shared task model

Tasks exist exactly once under `sharedTasks.tasks` in the active JSON file. SentinelTasks does not persist a second independent timer/completion copy.

A task node uses the same global task ID and stores only tree-specific metadata such as parent placement and description:

```json
{
  "sharedTasks": {
    "version": 1,
    "tasks": [
      {
        "id": "opengl",
        "name": "Study OpenGL",
        "created_at_epoch": 1786300000,
        "elapsed_seconds": 3900,
        "running": false,
        "completed": false,
        "completed_at_epoch": 0,
        "time_fragments": []
      }
    ]
  },
  "sentinelTasks": {
    "version": 3,
    "nodes": [
      {
        "id": "graphics",
        "name": "Graphics",
        "type": "folder",
        "parent": ""
      },
      {
        "id": "opengl",
        "type": "task",
        "parent": "graphics",
        "description": "Read and implement rendering examples."
      }
    ]
  }
}
```

A task created in Sentinel appears at the SentinelTasks root the next time the dataset is loaded if it has no tree placement yet. A task created in SentinelTasks is written to the same canonical registry and appears in Sentinel on reload.

On the first load of an old pre-shared-task dataset, the old duplicated Sentinel/SentinelTasks task world is deliberately purged and replaced by an empty canonical registry. Sentinel/SentinelTasks settings and `statistics` project history are preserved.

## Tree model

- Folder/category nodes are SentinelTasks-only structural objects.
- Task nodes are leaves linked by their global task ID.
- Folders may contain folders and shared tasks.
- Task names, creation timestamps, timers, completion state, task colors, and time fragments come from the canonical shared task.
- Descriptions and parent placement belong to SentinelTasks.
- Folder colors remain local to SentinelTasks.

## Commands

```text
addFolder <id> <parent|root> <name>
addTask <id> <parent|root> <name>
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

`erase`/`remove` recursively remove a folder and its descendants. Because task identities are shared, erasing a task removes that canonical task from the active task world when the change is saved.

`clear` without arguments deletes every folder and task and does not open a popup. `clear <id>` deletes one node/subtree.

`unset` clears completion state and its completion timestamp while preserving accumulated tracked time and time fragments.

## Selection

`manualSelect` immediately enters arrow/mouse selection mode:

```text
Up / Down    previous / next visible node
Left         parent
Right        first child
Mouse click  select node
Mouse wheel  scroll
Enter / Esc  leave manual mode
```

`select` opens the popup node selector. `select <id>` selects directly.

## Description editor

`setDescription` edits the selected node in the right-hand pane. `F2` saves, `Esc` cancels, and normal cursor/mouse editing is supported. Direct assignment remains available:

```text
setDescription opengl "Read the rendering chapter and implement the examples."
```

## Task timing

Every `start -> stop` or `start -> done` interval is recorded as its own `time_fragments` entry in the canonical shared task. The accumulated timer remains available as before. SentinelStats uses these fragments to draw the work-session timeline.

## Persistence and JSON selection

SentinelTasks defaults to `current_data.json` and uses the shared locked + atomic datastore. Its own `sentinelTasks` section stores tree layout/settings, while `sharedTasks.tasks` owns all task state.

Open another dataset with:

```text
setJsonFile
```

or directly:

```text
setJsonFile demo/demo_data.json
setJsonFile "projects/my tasks.json"
```

## Build

On Debian/Ubuntu/Linux Mint:

```bash
sudo apt install libncurses-dev libgtk-3-dev pkg-config cmake g++
cmake -S . -B build
cmake --build build
./build/bin/SentinelTasks
```
