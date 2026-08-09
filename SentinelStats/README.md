# SentinelStats

SentinelStats is the GTK 3 statistics/visualization companion for Sentinel and SentinelTasks. It reads `current_data.json` (or another selected JSON file) and visualizes recorded productivity history.

## Features

- GTK 3 desktop window.
- Summary metrics for total tasks, completed tasks, total tracked time, and projects.
- Cumulative completed-task line graph derived from task completion timestamps.
- Lines-of-code history graph with one line per project.
- `Open JSON` button to select another dataset at runtime.
- `Reload` button to reread the current file after Sentinel/SentinelTasks update it.
- Accepts a JSON path as the first command-line argument.

## Build dependencies

On Linux Mint / Ubuntu:

```bash
sudo apt install build-essential cmake pkg-config libgtk-3-dev
```

The repository already vendors `nlohmann/json.hpp` under the shared `include/` directory.

Build all Sentinel applications:

```bash
cmake -S . -B build
cmake --build build
```

Run with the default `current_data.json`:

```bash
./build/bin/SentinelStats
```

Or select a data file directly:

```bash
./build/bin/SentinelStats path/to/current_data.json
```

## Shared JSON shape

SentinelStats accepts the proposed shared-data layout:

```json
{
  "sentinel": {
    "tasks": [
      {
        "id": "coding",
        "name": "Programming",
        "elapsed_seconds": 5400,
        "running": false,
        "completed": true,
        "completed_at_epoch": 1786300000
      }
    ]
  },
  "sentinelTasks": {
    "nodes": [
      {
        "id": "opengl",
        "type": "task",
        "elapsed_seconds": 3200,
        "running": true,
        "completed": false,
        "completed_at_epoch": 0
      }
    ]
  },
  "statistics": {
    "projects": [
      {
        "id": "sentinel",
        "name": "Sentinel",
        "loc_history": [
          { "epoch": 1786200000, "lines": 1200 },
          { "epoch": 1786250000, "lines": 1640 },
          { "epoch": 1786300000, "lines": 2085 }
        ]
      },
      {
        "id": "game",
        "name": "Game Project",
        "loc_history": [
          { "epoch": 1786200000, "lines": 4200 },
          { "epoch": 1786300000, "lines": 5100 }
        ]
      }
    ]
  }
}
```

The existing flat Sentinel shape with a top-level `tasks` array is also accepted.

## Why LOC history is explicit

A current LOC count cannot reconstruct past LOC values. To draw a truthful progression function, each project needs timestamped snapshots in `loc_history`. A future Sentinel component can automatically sample project directories or Git repositories and append these snapshots to the shared JSON file.

Completed-task history is derived from each completed task's `completed_at_epoch`/`completion_epoch` timestamp and rendered as a cumulative line.
