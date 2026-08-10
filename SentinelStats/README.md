# SentinelStats

SentinelStats is the GTK 3 statistics and visualization companion for Sentinel and SentinelTasks. It reads the canonical shared task registry from `current_data.json` or another selected JSON dataset.

## Shared identity

Tasks exist once under `sharedTasks.tasks`. SentinelStats reads that registry once, so a task visible in both terminal applications is not double-counted.

Each task may contain creation/completion timestamps, cumulative elapsed time, and individual `time_fragments` sessions. These records drive the analytics views.

## Views

The GTK notebook currently contains:

- **Task progression** — cumulative task creation and completion lines.
- **Daily** — tracked hours by local calendar day for the latest 31 active days.
- **Weekly** — tracked hours by Monday-based local week for the latest 26 active weeks.
- **Tasks** — a table of task ID, name, tracked time, fragment count, state, creation time, and completion time. Column headers are clickable for sorting.
- **Time fragments** — scrollable per-task work-session timeline.
- **Lines of code** — project LOC history from explicit `statistics.projects[].loc_history` data.

The summary row shows total tasks, completed tasks, tracked time, fragment count, and configured project count.

## Daily and weekly aggregation

Daily/weekly analytics are derived from actual measured `time_fragments`, not reconstructed from the final cumulative timer. A session crossing local midnight is split between both calendar days. Weekly totals use Monday as the first day of the week.

For example, a fragment from 23:30 to 01:30 contributes 30 minutes to the first day and 90 minutes to the second day.

## Task table

The Tasks tab provides one row for every canonical shared task:

```text
ID       Task             Tracked   Fragments   State       Created            Completed
opengl   Modern OpenGL    4h 32m    7           running     2026-08-08 10:15   -
math     Linear Algebra   6h 12m    11          completed   2026-08-07 09:00   2026-08-10 18:30
```

## Build and tests

```bash
sudo apt install build-essential cmake pkg-config libgtk-3-dev libncurses-dev
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/bin/SentinelStats
```

Use the canonical demo dataset with:

```bash
./build/bin/SentinelStats demo/demo_data.json
```

`SentinelStats.DataAggregation` tests per-task analytics, fragment totals, splitting a session across midnight, and Monday-based weekly aggregation.

## Lines-of-code history

LOC history is still explicit data under `statistics.projects[].loc_history`. SentinelStats visualizes it but does not yet automatically sample project directories or Git repositories.
