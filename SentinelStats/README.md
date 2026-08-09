# SentinelStats

SentinelStats is the GTK 3 statistics/visualization companion for Sentinel and SentinelTasks. It reads `current_data.json` (or another selected JSON file) and visualizes recorded productivity history.

## Features

- GTK 3 desktop window.
- Summary metrics for total tasks, completed tasks, total tracked time, measured time fragments, and projects.
- Task-progression graph showing cumulative task creation and completion over time.
- Scrollable time-fragment timeline with one row per task and one bar per continuous `start -> stop/done` work session.
- Lines-of-code history graph with one line per project.
- `Open JSON` button to select another dataset at runtime.
- `Reload` button to reread the current file after Sentinel/SentinelTasks update it.
- Accepts a JSON path as the first command-line argument.

## Task creation timestamps and time fragments

New Sentinel and SentinelTasks tasks record the timestamp at which they are created. Each continuous timer run is also stored independently instead of retaining only one accumulated elapsed-time value.

A typical task now contains:

```json
{
  "id": "coding",
  "name": "Programming",
  "created_at_epoch": 1786300000,
  "elapsed_seconds": 5400,
  "running": false,
  "completed": true,
  "completed_at_epoch": 1786310000,
  "time_fragments": [
    {
      "started_at_epoch": 1786301000,
      "ended_at_epoch": 1786302800,
      "duration_seconds": 1800
    },
    {
      "started_at_epoch": 1786306000,
      "ended_at_epoch": 1786309600,
      "duration_seconds": 3600
    }
  ]
}
```

This means a total such as `01:30:00` can be represented truthfully as several separate work periods rather than one artificial continuous block.

While a task is running, its current fragment is saved with `ended_at_epoch` equal to `0` and an up-to-date `duration_seconds`. If the application is restarted, that historical fragment is closed at its last measured duration and a new fragment begins. Application downtime is therefore not counted as productive time.

Older JSON files remain loadable. If they predate this schema, their creation timestamp and fragment history are simply unknown; Sentinel does not fabricate historical timestamps.

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

SentinelStats accepts the shared-data layout:

```json
{
  "sentinel": {
    "tasks": [
      {
        "id": "coding",
        "name": "Programming",
        "created_at_epoch": 1786300000,
        "elapsed_seconds": 5400,
        "running": false,
        "completed": true,
        "completed_at_epoch": 1786310000,
        "time_fragments": [
          {
            "started_at_epoch": 1786301000,
            "ended_at_epoch": 1786302800,
            "duration_seconds": 1800
          }
        ]
      }
    ]
  },
  "sentinelTasks": {
    "nodes": [
      {
        "id": "opengl",
        "type": "task",
        "created_at_epoch": 1786300500,
        "elapsed_seconds": 3200,
        "running": true,
        "completed": false,
        "completed_at_epoch": 0,
        "time_fragments": [
          {
            "started_at_epoch": 1786307000,
            "ended_at_epoch": 0,
            "duration_seconds": 800
          }
        ]
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
      }
    ]
  }
}
```

The existing flat Sentinel shape with a top-level `tasks` array is also accepted.

## Graphs

### Task progression

The task-progression tab plots two cumulative series:

- **Created** — derived from `created_at_epoch`.
- **Completed** — derived from `completed_at_epoch` / `completion_epoch`.

### Time fragments

The time-fragments tab is a timeline. Every task with fragment data receives a horizontal row. Each recorded timer session is drawn as a bar positioned according to its real start and end timestamps. A fragment that was still open at the most recent save is marked at its right edge.

The view grows vertically with the number of measured tasks and is contained in a GTK scroll view.

### Lines of code

A current LOC count cannot reconstruct past LOC values. To draw a truthful progression function, each project needs timestamped snapshots in `loc_history`. A future Sentinel component can automatically sample project directories or Git repositories and append these snapshots to the shared JSON file.
