# SentinelEditor

SentinelEditor is a GTK+ 3 graphical text editor written in C++20. Its document is a native multiline `GtkTextView` with a synchronized line-number gutter, Vim-inspired modal controls, programming-speed metrics, native menus, and a fuzzy command palette.

## Run

```bash
./build/bin/SentinelEditor
./build/bin/SentinelEditor notes.txt
./build/bin/SentinelEditor src/main.cpp
```

## Main window

```text
File   Settings   Help
-------------------------------------------------------------------
  1 | #include <iostream>
  2 |
  3 | int main()
  4 | {
  5 |     return 0;
  6 | }
-------------------------------------------------------------------
NORMAL  main.cpp [+]                     LPM 3  LPH 47  CPM30 186
```

The editor field is a native `GtkTextView`, not a terminal emulator. GTK provides scrolling, mouse placement, selection, clipboard behavior, and Unicode text representation.

## Command Palette

Press:

```text
Ctrl+Shift+P
```

or choose:

```text
Settings -> Command Palette...
```

The palette behaves similarly to modern editor command palettes: typing a partial/subsequence query immediately fuzzy-ranks commands. For example:

```text
save
stng
line num
vim
```

can match actions such as `File: Save`, `Settings: Open Settings`, `View: Toggle Line Numbers`, and `Editor: Toggle Vim Mode`.

Use Up/Down to change the selected command, Enter to execute, and Escape to close the palette. The palette searches both command names and descriptions.

Currently registered palette actions include:

```text
File: New
File: Open
File: Save
File: Save As
File: Quit
Editor: Find
Settings: Open Settings
View: Toggle Line Numbers
View: Toggle Typing Metrics
Editor: Toggle Vim Mode
Help: Keyboard Shortcuts
Help: About SentinelEditor
```

The fuzzy-ranking algorithm is isolated in `FuzzySearch`, so it can be tested without GTK.

## Menu bar

### File

```text
New
Open...
Save
Save As...
Quit
```

These actions use the same underlying editor operations as the command palette and Ex-style commands. Open and Save As use GTK file chooser dialogs. Unsaved work is protected before destructive close/open operations.

### Settings

```text
Command Palette...    Ctrl+Shift+P
Settings...
--------------------
Toggle Line Numbers
Toggle Typing Metrics
Toggle Vim Mode
```

### Help

```text
Keyboard Shortcuts
About SentinelEditor
```

## Settings dialog

Choose:

```text
Settings -> Settings...
```

or run `Settings: Open Settings` through the command palette.

The separate settings popup currently supports:

- editor font size (8-32 pt);
- show/hide line-number gutter;
- show/hide LPM / LPH / CPM30 typing metrics;
- enable/disable Vim-style modal editing.

`Apply` applies changes without closing the dialog. `OK` applies and closes. When Vim mode is disabled, the central `GtkTextView` behaves as a conventional directly editable GTK text field.

## Vim-inspired modes

When Vim mode is enabled:

```text
NORMAL
INSERT
COMMAND
SEARCH
```

Normal-mode controls include:

```text
h j k l / arrows    move
w / b                next / previous word
0 / $                line start / end
gg / G               document start / end
i / a / I / A        enter Insert mode
o / O                create line and insert
x                     delete character
dd                    delete current line
/                     search
:                     Ex-style command entry
```

`Esc` returns from Insert to Normal mode.

## Ex-style commands

```text
:w
:w <path>
:q
:q!
:wq
:x
:e <path>
:e! <path>
:new
:new!
:settings
:palette
:help
```

## Line-number gutter

The gutter is a separate GTK drawing component beside the `GtkTextView`. It follows vertical scrolling, highlights the cursor line, redraws after edits/cursor moves, and automatically widens as line-number digit counts increase.

## Programming-speed metrics

The bottom-right status area shows:

```text
LPM 3   LPH 47   CPM30 186
```

- `LPM`: new-line creation events in the latest 60 seconds.
- `LPH`: new-line creation events in the latest 60 minutes.
- `CPM30`: direct character input during the latest 30 seconds, normalized to characters/minute.

## Source layout

```text
SentinelEditor/
├── CMakeLists.txt
├── main.cpp
├── include/
│   ├── Application.hpp
│   ├── EditorBuffer.hpp
│   ├── FuzzySearch.hpp
│   ├── LineNumberGutter.hpp
│   └── TypingMetrics.hpp
├── src/
│   ├── Application.cpp
│   ├── EditorBuffer.cpp
│   ├── FuzzySearch.cpp
│   ├── LineNumberGutter.cpp
│   └── TypingMetrics.cpp
└── tests/
    ├── EditorBufferTest.cpp
    ├── FuzzySearchTest.cpp
    └── TypingMetricsTest.cpp
```

## Build and test

```bash
sudo apt install build-essential cmake pkg-config libgtk-3-dev libncurses-dev
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/bin/SentinelEditor
```

CTest includes:

```text
SentinelEditor.Buffer
SentinelEditor.FuzzySearch
SentinelEditor.TypingMetrics
```

The workspace still requires ncurses because Sentinel and SentinelTasks remain terminal applications.
