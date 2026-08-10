# SentinelEditor

SentinelEditor is a Vim-inspired graphical text editor written in C++20 with GTK+ 3. The document is edited in a native multiline `GtkTextView` inside a scrollable GTK window; it is not a terminal emulator and no ncurses rendering is used by SentinelEditor anymore.

## Run

```bash
./build/bin/SentinelEditor
./build/bin/SentinelEditor notes.txt
./build/bin/SentinelEditor src/main.cpp
```

Opening a path that does not exist creates an empty named buffer. The file is created only when it is written.

## GTK layout

The main window contains:

```text
New | Open | Save | Save As                         SentinelEditor
------------------------------------------------------------------
  1 | #include <iostream>
  2 |
  3 | int main()
  4 | {
  5 |     return 0;
  6 | }
    |
    |                 native GtkTextView document
    |
------------------------------------------------------------------
NORMAL  main.cpp [+]  5:8               LPM 3  LPH 47  CPM30 186
: command/search entry (shown only when needed)
```

The large central `GtkTextView` is the actual editor. GTK owns text layout, scrolling, mouse positioning, selection, clipboard behavior and UTF-8 text representation.

A dedicated GTK line-number gutter is rendered to the left of the text view. It tracks the text view's vertical scroll position and uses GTK's text-line geometry, so line numbers remain aligned with the corresponding logical lines. The current cursor line is highlighted in the gutter.

The gutter width grows automatically as the document passes line-count digit boundaries such as 99 -> 100 or 999 -> 1000.

## Modes

SentinelEditor preserves four Vim-inspired modes:

```text
NORMAL
INSERT
COMMAND
SEARCH
```

In `NORMAL` mode the main `GtkTextView` is non-editable and key presses are interpreted as Vim-like commands. In `INSERT` mode the exact same `GtkTextView` becomes directly editable, so normal GTK text-input behavior is available. `Esc` returns to Normal mode.

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
gg              first position in document
G               end of document
Ctrl-B / PgUp   move upward by a page-sized step
Ctrl-F / PgDn   move downward by a page-sized step
```

Editing:

```text
i       enter Insert mode at cursor
a       enter Insert mode after cursor
I       insert at beginning of line
A       insert at end of line
o       create line below and insert
O       create line above and insert
x       delete character
dd      delete current line
```

Search and commands:

```text
/       open the GTK search entry
n       repeat previous search
:       open the GTK Ex-style command entry
```

`Ctrl-S` saves from Normal or Insert mode.

## Insert mode

Insert mode uses GTK's native text editing. Ordinary text entry, Unicode text, mouse placement, selection, clipboard shortcuts, scrolling, Enter, Backspace, Delete, Home/End and arrow keys are handled by the `GtkTextView`.

The editor records direct keyboard character/new-line events for its programming-speed metrics. Clipboard pastes are intentionally not treated as typing speed.

## Programming speed metrics

The bottom-right status area continuously shows:

```text
LPM 3   LPH 47   CPM30 186
```

- `LPM` — new-line creation events during the most recent 60 seconds.
- `LPH` — new-line creation events during the most recent 60 minutes.
- `CPM30` — direct character key presses during the most recent 30 seconds, normalized to characters per minute.

A line event is recorded for Enter in Insert mode and for Vim-style `o` / `O`. The rolling display refreshes every 200 ms, so values fall naturally when events leave their time windows.

## Native file controls

The toolbar provides:

```text
New
Open
Save
Save As
```

`Open` and `Save As` use native GTK file chooser dialogs. Closing the window or opening another file while the document is modified presents a GTK Save / Discard / Cancel warning.

## Command mode

Press `:` to reveal the command entry. Supported commands are:

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

`:q` and `:e` protect unsaved modifications. Their `!` forms deliberately discard them.

Paths containing spaces can be quoted:

```text
:w "notes/my file.txt"
:e "src/example file.cpp"
```

## Search

Press `/` to reveal the GTK search entry, type a literal string, and press Enter. The matching range is selected in the main text field and scrolled into view. `n` searches for the next occurrence and wraps to the beginning.

## Line-number gutter

`LineNumberGutter` is intentionally separate from the editor controller. It owns a GTK drawing area and listens to:

```text
GtkAdjustment::value-changed   vertical scrolling
GtkTextBuffer::changed         inserted/deleted lines and gutter width
GtkTextBuffer::mark-set        cursor/current-line changes
GtkWidget::size-allocate       text-view geometry changes
```

For each repaint it obtains the text view's visible buffer rectangle and the y-range for each visible logical line, then renders the corresponding 1-based line number. The line number of the insertion cursor is drawn at full emphasis with a subtle highlighted row behind it.

## Source layout

```text
SentinelEditor/
├── CMakeLists.txt
├── main.cpp
├── include/
│   ├── Application.hpp
│   ├── EditorBuffer.hpp
│   ├── LineNumberGutter.hpp
│   └── TypingMetrics.hpp
├── src/
│   ├── Application.cpp
│   ├── EditorBuffer.cpp
│   ├── LineNumberGutter.cpp
│   └── TypingMetrics.cpp
└── tests/
    ├── EditorBufferTest.cpp
    └── TypingMetricsTest.cpp
```

`Application` owns the GTK window, `GtkTextView`, Vim-inspired mode controller, native dialogs, search/command entries, status bar and key dispatch. `LineNumberGutter` owns the scroll-synchronized GTK line-number view. `EditorBuffer` remains the file-storage model and can synchronize complete GTK text through `Text()` / `SetText()`. `TypingMetrics` owns the rolling activity windows.

## Build

SentinelEditor links GTK3 rather than ncurses. From the repository root:

```bash
sudo apt install build-essential cmake pkg-config libgtk-3-dev libncurses-dev
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
./build/bin/SentinelEditor
ctest --test-dir build --output-on-failure
```

The CTest suite includes `SentinelEditor.Buffer` and `SentinelEditor.TypingMetrics`. The workspace still needs ncurses because Sentinel and SentinelTasks remain terminal applications.

## Natural next features

Useful next editor-specific additions include:

```text
syntax highlighting
undo / redo history
visual mode
yank / paste registers
multiple tabs / buffers
split editor panes
regex search and replace
project/file sidebar
configurable fonts and tab width
```
