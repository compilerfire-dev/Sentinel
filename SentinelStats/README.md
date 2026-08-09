# SentinelStats

SentinelStats is the GTK 3 statistics/visualization companion for Sentinel and SentinelTasks. It reads `current_data.json` (or another selected JSON file) and visualizes the canonical shared task history.

## Current task identity model

Tasks now exist exactly once under the root `sharedTasks.tasks` registry:

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
    "nodes": [
      {
        "id": "opengl",
        "type": "task",
        "parent": "graphics",
        "description": "Read and implement the rendering examples."
      }
    ]
  }
}
```

`Sentinel` is a flat view of the shared registry. `SentinelTasks` stores hierarchy and descriptions around the same global task ID; it does not own another timer/completion copy. SentinelStats therefore counts each logical task once.

When Sentinel or SentinelTasks opens a pre-shared-task dataset for the first time, the old duplicated task world is deliberately purged and a new empty `sharedTasks` registry is created. Non-task settings and statistics/project history are preserved. This avoids guessing how two previously independent task collections should be merged.

## Features

- Summary metrics for total tasks, completed tasks, total tracked time, measured time fragments, and projects.
- Task creation/completion progression graph.
- Scrollable task-fragment timeline where each bar is one continuous `start -> stop/done` session.
- Lines-of-code history graph with one line per configured project.
- `Open JSON` and `Reload` controls.
- JSON path accepted as the first command-line argument.
- Shared locked/atomic JSON reads through `SentinelShared::JsonDataStore`.

## Build

On Debian/Ubuntu/Linux Mint:

```bash
sudo apt install build-essential cmake pkg-config libgtk-3-dev libncurses-dev
cmake -S . -B build
cmake --build build
./build/bin/SentinelStats
```

Use the canonical demo dataset with:

```bash
./build/bin/SentinelStats demo/demo_data.json
```

## Lines-of-code history

LOC history is still explicit data under `statistics.projects[].loc_history`. SentinelStats visualizes it but does not yet automatically sample project directories or Git repositories.
