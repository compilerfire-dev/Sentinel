# Sentinel

Sentinel is a C++20 productivity suite composed of three applications:

- `Sentinel` — flat ncurses task/timer view.
- `SentinelTasks` — hierarchical ncurses folder/task planner.
- `SentinelStats` — GTK 3 + Cairo statistics and timeline visualizer.

## Canonical shared task identity

Tasks now exist exactly once per active JSON dataset under:

```text
sharedTasks.tasks
```

Both terminal applications operate on the same global task IDs, names, timing state, completion state, creation timestamp, colors, and time fragments.

`Sentinel` is the flat view of all shared tasks.

`SentinelTasks` stores folders and tree-specific placement/description metadata around those same task IDs. A task created in Sentinel appears at SentinelTasks root on the next dataset load if no tree placement exists yet. A task created in SentinelTasks appears in Sentinel on reload.

SentinelStats reads the canonical registry once, so one logical task is no longer double-counted merely because it is visible in both terminal applications.

### Shared JSON shape

```json
{
  "sharedTasks": {
    "version": 1,
    "tasks": [
      {
        "id": "math",
        "name": "Study linear algebra",
        "created_at_epoch": 1786300000,
        "elapsed_seconds": 3900,
        "running": false,
        "completed": false,
        "completed_at_epoch": 0,
        "time_fragments": [
          {
            "started_at_epoch": 1786301000,
            "ended_at_epoch": 1786302500,
            "duration_seconds": 1500
          }
        ]
      }
    ]
  },
  "sentinel": {
    "version": 5,
    "auto_save_seconds": 20,
    "defined_colors": {}
  },
  "sentinelTasks": {
    "version": 3,
    "auto_save_seconds": 20,
    "nodes": [
      {
        "id": "study",
        "name": "Study",
        "type": "folder",
        "parent": ""
      },
      {
        "id": "math",
        "type": "task",
        "parent": "study",
        "description": "Work through the current chapter."
      }
    ]
  },
  "statistics": {
    "projects": []
  }
}
```

### One-time task-world purge

The previous repository versions allowed Sentinel and SentinelTasks to own independent task collections. There is no reliable general rule for deciding whether two old records represented the same real task.

Therefore, when Sentinel or SentinelTasks opens a pre-shared-task dataset for the first time, the migration deliberately:

```text
creates sharedTasks.tasks = []
removes legacy Sentinel task arrays
clears the old SentinelTasks node tree
preserves Sentinel/SentinelTasks settings
preserves statistics/project history
```

This is intentionally destructive for the old task world and prevents ambiguous duplicate identities from being carried into the new model.

`demo/demo_data.json` has already been converted to the canonical schema and is not purged.

## Time tracking

Each task contains both a cumulative elapsed timer and individual measured work fragments.

```text
start math
... work ...
stop math
... later ...
start math
... work ...
done math
```

creates separate `time_fragments` entries for each continuous work session. SentinelStats displays these on the `Time fragments` timeline.

Every newly created task also records `created_at_epoch`, allowing SentinelStats to graph task creation versus completion over time.

## Sentinel commands

The main flat tracker includes commands such as:

```text
add <id> <name>
remove <id | fuzzy name>
erase <id | fuzzy name>
clear
clear <id | fuzzy name>
start <id | fuzzy name>
stop <id | fuzzy name>
done <id | fuzzy name>
unset <id | fuzzy name>
search <fuzzy text>
autoSave <duration>
setJsonFile
setJsonFile <path.json>
defineColor <id> rgb(r,g,b)
color <fg> bg <bg>
color <task-id> <fg> bg <bg>
commands
list
quit
```

A running Sentinel task is rendered as black text on a white task row rather than using a dedicated running-state character.

`autoSave` accepts compound durations such as:

```text
autoSave 20s
autoSave 1m
autoSave 10m 30s
```

## SentinelTasks

SentinelTasks adds folder hierarchy, descriptions, manual arrow/mouse selection, popup selection, recursive removal, scrolling, and the same shared task timers.

See `SentinelTasks/README.md` for its command details.

## SentinelStats

SentinelStats currently visualizes:

- total/completed/running task data from the canonical registry;
- total tracked time;
- number of measured work fragments;
- task creation/completion progression;
- per-task work-session fragments on a scrollable timeline;
- explicit project `loc_history` series.

Automatic LOC sampling is not yet implemented.

## Shared locked + atomic persistence

All applications use `SentinelShared::JsonDataStore` for shared JSON access.

Writes use:

```text
sidecar flock
read latest JSON while locked
modify transaction
write same-directory temporary file
fsync temporary file
atomic rename
fsync parent directory
unlock
```

This prevents Sentinel and SentinelTasks from observing or producing partially written JSON files and protects section-level read/modify/write transactions.

## File selection

Bare:

```text
setJsonFile
```

opens the native GTK JSON file chooser in Sentinel and SentinelTasks. A direct path remains supported:

```text
setJsonFile demo/demo_data.json
```

## Build

On Debian/Ubuntu/Linux Mint:

```bash
sudo apt install libncurses-dev libgtk-3-dev pkg-config cmake g++
cmake -S . -B build
cmake --build build
```

Run:

```bash
./build/bin/Sentinel
./build/bin/SentinelTasks
./build/bin/SentinelStats
./build/bin/SentinelStats demo/demo_data.json
```

Run the persistence tests with:

```bash
ctest --test-dir build --output-on-failure
```

The repository vendors `nlohmann/json.hpp` under `include/`.
