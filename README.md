# Sentinel

Sentinel is a C++20 productivity suite composed of four applications:

- `Sentinel` — flat ncurses task/timer view.
- `SentinelTasks` — hierarchical ncurses folder/task planner.
- `SentinelStats` — GTK 3 + Cairo statistics and timeline visualizer.
- `SentinelEditor` — Vim-inspired modal ncurses text editor.

## Canonical shared tasks

Tasks exist exactly once per active JSON dataset under:

```text
sharedTasks.tasks
```

Both terminal task applications operate on the same global task IDs, names, timing state, completion state, creation timestamp, colors, and measured time fragments. SentinelTasks adds tree placement and descriptions around those same identities. SentinelStats reads the canonical registry once, preventing duplicate statistics.

The shared persistence layer uses sidecar `flock`, locked read-modify-write transactions, same-directory temporary files, `fsync`, atomic rename, and optimistic per-task merging/conflict detection.

## Time tracking

Each task contains both cumulative elapsed time and individual work sessions:

```text
start math
... work ...
stop math
... later ...
start math
... work ...
done math
```

Each continuous session becomes a separate `time_fragments` entry. Newly created tasks also record `created_at_epoch` and completed tasks record `completed_at_epoch`.

## Sentinel

The flat tracker supports task creation/removal, start/stop/done/unset, fuzzy task resolution, scrolling, colors, configurable autosave, JSON switching, and shared canonical task persistence.

Examples:

```text
add math "Study linear algebra"
start math
stop math
done math
unset math
autoSave 10m 30s
```

## SentinelTasks

SentinelTasks adds hierarchy and tree editing. Important commands include:

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
setDescription <id> <description>
```

`move` rejects cyclic hierarchy. `rename` changes only the display name, keeping the global task ID stable. Folder collapse state is persisted; `[+]` marks collapsed folders and `[-]` expanded folders. Hidden descendants remain in persistence and canonical task storage.

See `SentinelTasks/README.md` for details.

## SentinelStats

SentinelStats currently provides:

- summary task/time/fragment metrics;
- task creation/completion progression;
- daily tracked-time bar chart;
- Monday-based weekly tracked-time chart;
- sortable task analytics table;
- scrollable per-task work-session timeline;
- explicit project LOC history visualization.

Daily analytics split sessions at local midnight, so work is attributed to the actual calendar days on which it occurred.

Automatic LOC sampling is still not implemented.

## SentinelEditor

SentinelEditor is a separate Vim-inspired ncurses text editor. Its first version includes Normal, Insert, Command, and Search modes, line numbers, vertical/horizontal viewport management, mouse placement/wheel scrolling, and explicit file saving.

Typical controls:

```text
h j k l / arrows    move
i a I A o O          enter Insert mode
w b 0 $ gg G         Vim-style movement
x / dd               delete character / line
/                    search
n                    repeat search
:                    Ex-style command mode
```

Supported Ex-style commands include:

```text
:w
:w <path>
:q
:q!
:wq
:e <path>
:e! <path>
:new
:new!
:help
```

See `SentinelEditor/README.md` for the complete first-version key map and architecture.

## Build

On Debian/Ubuntu/Linux Mint:

```bash
sudo apt install build-essential cmake pkg-config libncurses-dev libgtk-3-dev
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
```

Run:

```bash
./build/bin/Sentinel
./build/bin/SentinelTasks
./build/bin/SentinelStats
./build/bin/SentinelStats demo/demo_data.json
./build/bin/SentinelEditor
./build/bin/SentinelEditor README.md
```

## Tests and CI

Run all tests locally with:

```bash
ctest --test-dir build --output-on-failure
```

The test suite includes shared datastore/concurrency tests, shared-task migration and merge tests, SentinelTasks tree manipulation/collapse tests, SentinelStats aggregation tests, and the SentinelEditor text-buffer regression test.

GitHub Actions runs configure, build, and CTest automatically on every push and pull request using `.github/workflows/ci.yml`.

The repository vendors `nlohmann/json.hpp` under `include/`.
