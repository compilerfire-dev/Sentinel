# SentinelEditor

SentinelEditor is a Vim-inspired terminal text editor written in C++20 with ncurses. It is intentionally a smaller modal editor rather than a full Vim clone, with the source split into a testable text buffer, rolling typing metrics, and an ncurses application layer.

## Run

```bash
./build/bin/SentinelEditor
./build/bin/SentinelEditor notes.txt
./build/bin/SentinelEditor src/main.cpp
```

Opening a path that does not exist creates an empty named buffer. The file is created only when it is written.

## Modes

SentinelEditor currently provides four modes:

```text
NORMAL
INSERT
COMMAND
SEARCH
```

`Esc` returns to Normal mode from Insert, Command, or Search mode.

## Normal mode

Movement:

```text
h / Left        move left
j / Down        move down
k / Up          move up
l / Right       move right
w               next word
b               previous word
0 / Home        beginning of line
$ / End         end of line
gg              first line
G               last line
Ctrl-B / PgUp   page upward
Ctrl-F / PgDn   page downward
```

Editing:

```text
i       insert at cursor
a       insert after cursor
I       insert at beginning of line
A       insert at end of line
o       create line below and insert
O       create line above and insert
x       delete character
dd      delete current line
```

Search and commands:

```text
/       search forward
n       repeat previous search
:       enter Ex-style command mode
```

The mouse can place the cursor in the text area. The mouse wheel scrolls vertically.

## Insert mode

Insert mode supports ordinary text entry plus:

```text
Arrow keys
Home / End
Enter
Backspace
Delete
Esc
```

Backspace at column zero joins the current line to the previous line. Delete at end-of-line joins the following line.

## Programming speed metrics

The bottom-right side of the reverse-video status line continuously shows rolling typing statistics:

```text
LPM 3 | LPH 47 | CPM30 186
```

The values mean:

- `LPM` — number of new-line creation events during the most recent 60 seconds.
- `LPH` — number of new-line creation events during the most recent 60 minutes.
- `CPM30` — character insertions observed during the most recent 30 seconds, normalized to characters per minute. For example, 93 inserted characters in 30 seconds displays as `CPM30 186`.

A line event is recorded when a new line is created with `Enter`, `o`, or `O`. Deleting or joining lines does not reduce the counters, because these metrics describe gross typing/output activity rather than net file growth.

Only characters successfully inserted into the edited text are counted. Navigation, command-mode typing, search queries, deletions, and mouse activity do not increase `CPM30`.

The metrics are session-wide across files opened within the same SentinelEditor process. They use `std::chrono::steady_clock` and rolling windows, and the ncurses UI refreshes periodically so the displayed rates naturally fall as old events leave their windows even while no key is pressed.

## Command mode

Supported commands:

```text
:w
:w <path>
:q
:q!
:wq
:wq <path>
:x
:e <path>
:e! <path>
:new
:new!
:help
```

`:q` and `:e` protect unsaved modifications. Their `!` variants deliberately discard them.

Paths containing spaces can be quoted:

```text
:w "notes/my file.txt"
:e "src/example file.cpp"
```

## Search

Press `/`, type a literal search string, and press Enter. `n` searches for the next occurrence and wraps to the beginning of the file.

## Layout

The editor renders line numbers on the left, a Vim-style reverse-video status line near the bottom, and a command/message line at the bottom. The right side of the status line is reserved for the live programming-speed metrics when the terminal is wide enough.

## Source layout

```text
SentinelEditor/
├── CMakeLists.txt
├── main.cpp
├── include/
│   ├── Application.hpp
│   ├── EditorBuffer.hpp
│   └── TypingMetrics.hpp
├── src/
│   ├── Application.cpp
│   ├── EditorBuffer.cpp
│   └── TypingMetrics.cpp
└── tests/
    ├── EditorBufferTest.cpp
    └── TypingMetricsTest.cpp
```

`EditorBuffer` owns text storage, file loading/saving, line splitting/joining, insertion and deletion. `TypingMetrics` owns the rolling 30-second, 60-second, and 60-minute activity windows. `Application` owns ncurses rendering, modes, key bindings, cursor state, scrolling, searching, Ex-style commands, and reporting successful edit events to the metrics model.

## Current limitations / natural next features

The first version deliberately does not attempt to implement every Vim subsystem. Useful future additions include:

```text
undo / redo
visual selection
yank / paste registers
syntax highlighting
configurable tab width
UTF-8 code-point-aware cursor movement
regex search and replacement
multiple buffers / tabs
split windows
marks
macros
command history
swap/recovery files
```

## Build

From the repository root:

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
./build/bin/SentinelEditor
ctest --test-dir build --output-on-failure
```

The CTest suite includes both `SentinelEditor.Buffer` and `SentinelEditor.TypingMetrics`.
