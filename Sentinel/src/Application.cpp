#include "Application.hpp"
#include "FuzzySearch.hpp"
#include "NativeFileDialog.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <iterator>
#include <limits>
#include <ncurses.h>
#include <sstream>
#include <string_view>
#include <utility>

namespace {

constexpr std::size_t MaxSuggestions = 6;
constexpr std::size_t MouseWheelStep = 3;
constexpr short ScreenColorPair = 1;
constexpr short TaskColorPair = 2;
constexpr short RunningTaskPair = 3;
constexpr short FirstPerTaskPair = 4;

std::size_t ContentScrollOffset = 0;

constexpr RgbColor ScreenForeground{255, 255, 255};
constexpr RgbColor ScreenBackground{0, 0, 0};
constexpr RgbColor RunningForeground{0, 0, 0};
constexpr RgbColor RunningBackground{255, 255, 255};

struct CommandDefinition {
    std::string_view name;
    std::string_view description;
};

struct DialogRect {
    int top{};
    int left{};
    int height{};
    int width{};
};

constexpr std::array<CommandDefinition, 16> Commands{{
    {"add", "add a new task with an ID"},
    {"remove", "remove a task"},
    {"erase", "erase/remove a task"},
    {"start", "start or resume a task"},
    {"stop", "stop a running task"},
    {"done", "complete a task"},
    {"unset", "untick a completed task"},
    {"search", "fuzzy-search tasks"},
    {"list", "show all tasks"},
    {"commands", "show all commands"},
    {"autoSave", "set autosave interval, e.g. 20s or 10m 30s"},
    {"setJsonFile", "choose/switch active JSON data file"},
    {"defineColor", "define a named RGB color"},
    {"color", "set default or per-task color"},
    {"help", "show command help"},
    {"quit", "save and exit Sentinel"},
}};

bool TakesTaskArgument(std::string_view command) {
    return command == "remove" || command == "erase" ||
           command == "start" || command == "stop" ||
           command == "done" || command == "unset" ||
           command == "search";
}

bool IsWheelUp(mmask_t state) {
#ifdef BUTTON4_PRESSED
    if ((state & BUTTON4_PRESSED) != 0) return true;
#endif
#ifdef BUTTON4_CLICKED
    if ((state & BUTTON4_CLICKED) != 0) return true;
#endif
    return false;
}

bool IsWheelDown(mmask_t state) {
#ifdef BUTTON5_PRESSED
    if ((state & BUTTON5_PRESSED) != 0) return true;
#endif
#ifdef BUTTON5_CLICKED
    if ((state & BUTTON5_CLICKED) != 0) return true;
#endif
    return false;
}

std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t");
    return value.substr(first, last - first + 1);
}

std::string TrimLeft(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    return first == std::string::npos ? std::string{} : value.substr(first);
}

std::string UnquotePartial(std::string value) {
    value = TrimLeft(std::move(value));
    if (!value.empty() && value.front() == '"') value.erase(value.begin());
    if (!value.empty() && value.back() == '"') value.pop_back();
    return value;
}

std::string QuoteIfNeeded(const std::string& value) {
    if (value.find_first_of(" \t\"") == std::string::npos) return value;

    std::string result = "\"";
    for (const char character : value) {
        if (character == '\\' || character == '"') result += '\\';
        result += character;
    }
    result += '"';
    return result;
}

DialogRect CalculateDialogRect(std::size_t fields) {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);

    DialogRect result;
    result.width = std::clamp(width * 3 / 5, 54, std::max(54, width - 4));
    result.height = std::clamp(
        9 + static_cast<int>(fields) * 2,
        11,
        std::max(11, height - 4)
    );
    result.left = std::max(0, (width - result.width) / 2);
    result.top = std::max(0, (height - result.height) / 2);
    return result;
}

int DialogFieldRow(const DialogRect& rect, std::size_t index) {
    return rect.top + 4 + static_cast<int>(index) * 2;
}

short NearestBasicColor(const RgbColor& color) {
    struct BasicColor {
        short ncursesColor;
        int red;
        int green;
        int blue;
    };

    constexpr std::array<BasicColor, 8> basic{{
        {COLOR_BLACK, 0, 0, 0},
        {COLOR_RED, 255, 0, 0},
        {COLOR_GREEN, 0, 255, 0},
        {COLOR_YELLOW, 255, 255, 0},
        {COLOR_BLUE, 0, 0, 255},
        {COLOR_MAGENTA, 255, 0, 255},
        {COLOR_CYAN, 0, 255, 255},
        {COLOR_WHITE, 255, 255, 255},
    }};

    long bestDistance = std::numeric_limits<long>::max();
    short best = COLOR_WHITE;

    for (const auto& candidate : basic) {
        const long red = color.red - candidate.red;
        const long green = color.green - candidate.green;
        const long blue = color.blue - candidate.blue;
        const long distance = red * red + green * green + blue * blue;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate.ncursesColor;
        }
    }
    return best;
}

short NearestTerminalColor(const RgbColor& color) {
    if (!has_colors() || COLORS <= 0) return NearestBasicColor(color);

    const int limit = std::min(
        COLORS,
        static_cast<int>(std::numeric_limits<short>::max()) + 1
    );
    long bestDistance = std::numeric_limits<long>::max();
    short best = NearestBasicColor(color);
    bool found = false;

    for (int index = 0; index < limit; ++index) {
        short red = 0;
        short green = 0;
        short blue = 0;
        if (color_content(static_cast<short>(index), &red, &green, &blue) == ERR) continue;

        const long redDistance = color.red - red * 255 / 1000;
        const long greenDistance = color.green - green * 255 / 1000;
        const long blueDistance = color.blue - blue * 255 / 1000;
        const long distance =
            redDistance * redDistance +
            greenDistance * greenDistance +
            blueDistance * blueDistance;

        if (!found || distance < bestDistance) {
            found = true;
            bestDistance = distance;
            best = static_cast<short>(index);
        }
    }
    return best;
}

bool InitializeColorPair(
    short pair,
    const RgbColor& foreground,
    const RgbColor& background
) {
    if (!has_colors() || pair <= 0 || pair >= COLOR_PAIRS) return false;
    return init_pair(
        pair,
        NearestTerminalColor(foreground),
        NearestTerminalColor(background)
    ) != ERR;
}

short NormalUiPair() {
    return has_colors() && COLOR_PAIRS > ScreenColorPair ? ScreenColorPair : 0;
}

void SetNormalUiAttributes() {
    attr_set(A_NORMAL, NormalUiPair(), nullptr);
}

void DrawClippedField(
    int row,
    int column,
    int maxWidth,
    const std::string& text
) {
    if (maxWidth > 0 && column >= 0) {
        mvaddnstr(row, column, text.c_str(), maxWidth);
    }
}

} // namespace

Application::Application()
    : commandProcessor_(taskManager_, displaySettings_),
      lastAutosave_(std::chrono::steady_clock::now()) {
    std::string error;
    if (!taskManager_.Load(error)) persistenceStatus_ = "Load failed: " + error;
}

int Application::Run() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    timeout(100);
    mousemask(ALL_MOUSE_EVENTS, nullptr);

    if (has_colors()) start_color();
    ApplyColors();

    while (!commandProcessor_.ShouldQuit()) {
        HandleInput();
        PeriodicAutosave();
        if (displaySettings_.dirty) ApplyColors();
        Render();
    }

    std::string error;
    taskManager_.Save(error);
    endwin();
    return 0;
}

void Application::PeriodicAutosave() {
    const auto now = std::chrono::steady_clock::now();
    if (now - lastAutosave_ < std::chrono::seconds(1)) return;

    lastAutosave_ = now;
    std::string error;
    if (!taskManager_.Save(error)) {
        persistenceStatus_ = "Autosave failed: " + error;
    }
}

void Application::ApplyColors() {
    displaySettings_.dirty = false;

    if (!has_colors()) {
        persistenceStatus_ = "Terminal does not support colors.";
        return;
    }

    const bool screenPairReady = InitializeColorPair(
        ScreenColorPair,
        ScreenForeground,
        ScreenBackground
    );
    const bool taskPairReady = InitializeColorPair(
        TaskColorPair,
        displaySettings_.foreground,
        displaySettings_.background
    );
    const bool runningPairReady = InitializeColorPair(
        RunningTaskPair,
        RunningForeground,
        RunningBackground
    );

    if (!screenPairReady) {
        persistenceStatus_ = "Terminal could not initialize the Sentinel screen color pair.";
        return;
    }
    if (!taskPairReady) {
        persistenceStatus_ = "Terminal could not initialize the default task color pair.";
    } else if (!runningPairReady) {
        persistenceStatus_ = "Terminal could not initialize the running-task highlight.";
    }

    wbkgd(stdscr, COLOR_PAIR(ScreenColorPair));
    wattr_set(stdscr, A_NORMAL, ScreenColorPair, nullptr);
    werase(stdscr);
    touchwin(stdscr);
    wnoutrefresh(stdscr);
    doupdate();
}

void Application::OpenNativeJsonFilePicker() {
    curs_set(0);
    const auto selected = SentinelShared::SelectJsonFile(taskManager_.GetJsonFile());

    if (selected) {
        const std::string command =
            "setJsonFile " + QuoteArgument(selected->string());
        commandProcessor_.Execute(command);
        AddCommandToHistory(command);
        ContentScrollOffset = 0;
        persistenceStatus_.clear();
    } else {
        persistenceStatus_ = "JSON file selection cancelled or unavailable.";
    }

    clearok(stdscr, TRUE);
    touchwin(stdscr);
    refresh();
    curs_set(1);
}

void Application::AddCommandToHistory(const std::string& command) {
    if (command.empty()) return;
    if (commandHistory_.empty() || commandHistory_.back() != command) {
        commandHistory_.push_back(command);
    }
    ResetHistoryNavigation();
}

void Application::RecallPreviousCommand() {
    if (commandHistory_.empty()) return;

    if (!historyIndex_) {
        commandBeforeHistory_ = commandBuffer_;
        historyIndex_ = commandHistory_.size() - 1;
    } else if (*historyIndex_ > 0) {
        --(*historyIndex_);
    }

    commandBuffer_ = commandHistory_[*historyIndex_];
    cursorPosition_ = commandBuffer_.size();
    ResetSuggestionSelection();
}

void Application::RecallNextCommand() {
    if (!historyIndex_) return;

    if (*historyIndex_ + 1 < commandHistory_.size()) {
        ++(*historyIndex_);
        commandBuffer_ = commandHistory_[*historyIndex_];
    } else {
        commandBuffer_ = commandBeforeHistory_;
        ResetHistoryNavigation();
    }

    cursorPosition_ = commandBuffer_.size();
    ResetSuggestionSelection();
}

void Application::ResetHistoryNavigation() {
    historyIndex_.reset();
    commandBeforeHistory_.clear();
}

void Application::HandleInput() {
    const int key = getch();
    if (key == ERR) return;

    if (commandDialog_) {
        if (key == KEY_MOUSE) {
            MEVENT event{};
            if (getmouse(&event) == OK) HandleCommandDialogMouse(event.x, event.y);
        } else {
            HandleCommandDialogInput(key);
        }
        return;
    }

    const auto suggestions = BuildSuggestions();

    if (key == KEY_MOUSE) {
        HandleMouse();
        return;
    }
    if (key == '\t') {
        AcceptSuggestion(suggestions);
        ResetHistoryNavigation();
        return;
    }
    if (key == KEY_LEFT) {
        if (cursorPosition_ > 0) --cursorPosition_;
        ResetHistoryNavigation();
        ResetSuggestionSelection();
        return;
    }
    if (key == KEY_RIGHT) {
        if (cursorPosition_ < commandBuffer_.size()) ++cursorPosition_;
        ResetHistoryNavigation();
        ResetSuggestionSelection();
        return;
    }
    if (key == KEY_UP) {
        if (navigatingSuggestions_ && !suggestions.empty()) {
            selectedSuggestion_ = selectedSuggestion_ == 0
                ? suggestions.size() - 1
                : selectedSuggestion_ - 1;
        } else {
            RecallPreviousCommand();
        }
        return;
    }
    if (key == KEY_DOWN) {
        if (navigatingSuggestions_ && !suggestions.empty()) {
            selectedSuggestion_ = (selectedSuggestion_ + 1) % suggestions.size();
        } else if (historyIndex_) {
            RecallNextCommand();
        } else if (!suggestions.empty()) {
            navigatingSuggestions_ = true;
            selectedSuggestion_ = 0;
        }
        return;
    }

    if (key == '\n' || key == KEY_ENTER) {
        if (navigatingSuggestions_ && !suggestions.empty()) {
            AcceptSuggestion(suggestions);
            return;
        }

        const std::string executed = Trim(commandBuffer_);
        const auto separator = executed.find_first_of(" \t");
        const std::string command = executed.substr(0, separator);

        if (!executed.empty() && separator == std::string::npos &&
            command == "setJsonFile") {
            AddCommandToHistory(command);
            commandBuffer_.clear();
            cursorPosition_ = 0;
            ResetSuggestionSelection();
            OpenNativeJsonFilePicker();
            return;
        }

        if (!executed.empty() && separator == std::string::npos &&
            OpenCommandDialog(command)) {
            commandBuffer_.clear();
            cursorPosition_ = 0;
            ResetSuggestionSelection();
            return;
        }

        commandProcessor_.Execute(executed);
        AddCommandToHistory(executed);
        commandBuffer_.clear();
        cursorPosition_ = 0;
        persistenceStatus_.clear();
        ResetSuggestionSelection();
        return;
    }

    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (cursorPosition_ > 0 && !commandBuffer_.empty()) {
            commandBuffer_.erase(cursorPosition_ - 1, 1);
            --cursorPosition_;
        }
        ResetHistoryNavigation();
        ResetSuggestionSelection();
        return;
    }

    if (key >= 32 && key <= 126) {
        commandBuffer_.insert(
            commandBuffer_.begin() + static_cast<std::ptrdiff_t>(cursorPosition_),
            static_cast<char>(key)
        );
        ++cursorPosition_;
        ResetHistoryNavigation();
        ResetSuggestionSelection();
    }
}

void Application::HandleMouse() {
    MEVENT event{};
    if (getmouse(&event) != OK) return;

    if (IsWheelUp(event.bstate) || IsWheelDown(event.bstate)) {
        const auto suggestions = BuildSuggestions();
        const int lastRow = SuggestionStartRow(suggestions.size());
        const std::size_t capacity = static_cast<std::size_t>(std::max(0, lastRow - 2));
        const std::size_t total = !commandProcessor_.GetInfoLines().empty()
            ? commandProcessor_.GetInfoLines().size()
            : VisibleTaskIndices().size();
        const std::size_t maxOffset = total > capacity ? total - capacity : 0;

        if (IsWheelUp(event.bstate)) {
            ContentScrollOffset = ContentScrollOffset > MouseWheelStep
                ? ContentScrollOffset - MouseWheelStep
                : 0;
        } else {
            ContentScrollOffset = std::min(
                maxOffset,
                ContentScrollOffset + MouseWheelStep
            );
        }
        return;
    }

    if ((event.bstate & BUTTON1_CLICKED) == 0 &&
        (event.bstate & BUTTON1_PRESSED) == 0) return;

    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)width;

    if (event.y == height - 1) {
        cursorPosition_ = std::min(
            commandBuffer_.size(),
            static_cast<std::size_t>(std::max(0, event.x - 2))
        );
        ResetHistoryNavigation();
        ResetSuggestionSelection();
        return;
    }

    const auto suggestions = BuildSuggestions();
    if (suggestions.empty()) return;
    const int start = SuggestionStartRow(suggestions.size());
    if (event.y >= start &&
        event.y < start + static_cast<int>(suggestions.size())) {
        selectedSuggestion_ = static_cast<std::size_t>(event.y - start);
        navigatingSuggestions_ = true;
        AcceptSuggestion(suggestions);
        ResetHistoryNavigation();
    }
}

void Application::AcceptSuggestion(
    const std::vector<Suggestion>& suggestions
) {
    if (suggestions.empty()) return;
    selectedSuggestion_ = std::min(selectedSuggestion_, suggestions.size() - 1);
    commandBuffer_ = suggestions[selectedSuggestion_].replacement;
    cursorPosition_ = commandBuffer_.size();
    ResetSuggestionSelection();
}

void Application::ResetSuggestionSelection() {
    selectedSuggestion_ = 0;
    navigatingSuggestions_ = false;
}

bool Application::OpenCommandDialog(const std::string& command) {
    CommandDialog dialog;
    dialog.command = command;
    dialog.title = "Command: " + command;

    auto text = [](std::string label, std::string value = {}) {
        DialogField field;
        field.label = std::move(label);
        field.value = std::move(value);
        field.cursor = field.value.size();
        return field;
    };

    auto drop = [](
        std::string label,
        std::vector<std::string> options,
        std::string preferred = {}
    ) {
        DialogField field;
        field.label = std::move(label);
        field.kind = DialogFieldKind::DropList;
        field.options = std::move(options);
        if (!field.options.empty()) {
            const auto iterator = std::find(
                field.options.begin(), field.options.end(), preferred
            );
            field.selectedOption = iterator == field.options.end()
                ? 0
                : static_cast<std::size_t>(
                    std::distance(field.options.begin(), iterator)
                );
            field.value = field.options[field.selectedOption];
        }
        return field;
    };

    if (command == "autoSave") {
        dialog.fields.push_back(text(
            "Duration",
            std::to_string(taskManager_.GetAutoSaveInterval().count()) + "s"
        ));
    } else if (command == "add") {
        dialog.fields.push_back(text("ID"));
        dialog.fields.push_back(text("Name"));
    } else if (
        command == "remove" || command == "erase" || command == "start" ||
        command == "stop" || command == "done" || command == "unset"
    ) {
        dialog.fields.push_back(drop("Task", TaskIdOptions()));
    } else if (command == "search") {
        dialog.fields.push_back(text("Query"));
    } else if (command == "defineColor") {
        dialog.fields.push_back(text("Color name"));
        dialog.fields.push_back(text("RGB", "rgb(255,255,255)"));
    } else if (command == "color") {
        auto targets = TaskIdOptions();
        targets.insert(targets.begin(), "default");
        dialog.fields.push_back(drop("Target", std::move(targets), "default"));
        dialog.fields.push_back(drop("Foreground", ColorNameOptions(), "white"));
        dialog.fields.push_back(drop("Background", ColorNameOptions(), "black"));
    } else {
        return false;
    }

    commandDialog_ = std::move(dialog);
    persistenceStatus_ = "Argument window opened for: " + command;
    return true;
}

void Application::CloseCommandDialog() {
    commandDialog_.reset();
    curs_set(1);
}

void Application::HandleCommandDialogInput(int key) {
    if (!commandDialog_) return;
    auto& dialog = *commandDialog_;

    if (key == 27) {
        CloseCommandDialog();
        return;
    }
    if (key == KEY_F(2)) {
        SubmitCommandDialog();
        return;
    }
    if (key == '\t') {
        MoveDialogFocus(1);
        return;
    }
#ifdef KEY_BTAB
    if (key == KEY_BTAB) {
        MoveDialogFocus(-1);
        return;
    }
#endif

    const std::size_t submit = dialog.fields.size();
    const std::size_t cancel = submit + 1;

    if (dialog.focusedControl == submit) {
        if (key == '\n' || key == KEY_ENTER || key == ' ') SubmitCommandDialog();
        return;
    }
    if (dialog.focusedControl == cancel) {
        if (key == '\n' || key == KEY_ENTER || key == ' ') CloseCommandDialog();
        return;
    }
    if (dialog.focusedControl >= dialog.fields.size()) return;

    auto& field = dialog.fields[dialog.focusedControl];
    if (field.kind == DialogFieldKind::DropList) {
        if (field.options.empty()) return;
        if (key == KEY_UP) {
            field.selectedOption = field.selectedOption == 0
                ? field.options.size() - 1
                : field.selectedOption - 1;
        } else if (key == KEY_DOWN) {
            field.selectedOption = (field.selectedOption + 1) % field.options.size();
        } else if (key == '\n' || key == KEY_ENTER) {
            MoveDialogFocus(1);
            return;
        }
        field.value = field.options[field.selectedOption];
        return;
    }

    if (key == KEY_LEFT) {
        if (field.cursor > 0) --field.cursor;
    } else if (key == KEY_RIGHT) {
        if (field.cursor < field.value.size()) ++field.cursor;
    } else if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (field.cursor > 0) {
            field.value.erase(field.cursor - 1, 1);
            --field.cursor;
        }
    } else if (key == '\n' || key == KEY_ENTER) {
        MoveDialogFocus(1);
    } else if (key >= 32 && key <= 126) {
        field.value.insert(
            field.value.begin() + static_cast<std::ptrdiff_t>(field.cursor),
            static_cast<char>(key)
        );
        ++field.cursor;
    }
}

void Application::HandleCommandDialogMouse(int mouseX, int mouseY) {
    if (!commandDialog_) return;
    auto& dialog = *commandDialog_;
    const auto rect = CalculateDialogRect(dialog.fields.size());
    const int labelWidth = std::min(18, std::max(10, rect.width / 4));
    const int inputColumn = rect.left + 3 + labelWidth;
    const int inputWidth = std::max(10, rect.width - labelWidth - 7);

    for (std::size_t index = 0; index < dialog.fields.size(); ++index) {
        if (mouseY == DialogFieldRow(rect, index) &&
            mouseX >= inputColumn && mouseX < inputColumn + inputWidth) {
            dialog.focusedControl = index;
            return;
        }
    }

    const int buttonRow = rect.top + rect.height - 3;
    const int submitColumn = rect.left + rect.width / 2 - 14;
    const int cancelColumn = rect.left + rect.width / 2 + 3;
    if (mouseY == buttonRow && mouseX >= submitColumn &&
        mouseX < submitColumn + 12) {
        SubmitCommandDialog();
    } else if (mouseY == buttonRow && mouseX >= cancelColumn &&
               mouseX < cancelColumn + 12) {
        CloseCommandDialog();
    }
}

void Application::MoveDialogFocus(int delta) {
    if (!commandDialog_) return;
    auto& dialog = *commandDialog_;
    const std::size_t count = dialog.fields.size() + 2;
    dialog.focusedControl = delta < 0
        ? (dialog.focusedControl == 0 ? count - 1 : dialog.focusedControl - 1)
        : (dialog.focusedControl + 1) % count;
}

bool Application::SubmitCommandDialog() {
    if (!commandDialog_) return false;
    for (const auto& field : commandDialog_->fields) {
        if (field.value.empty()) {
            commandDialog_->validationMessage =
                "Required field is empty: " + field.label;
            return false;
        }
    }

    const std::string line = BuildDialogCommand();
    if (line.empty()) return false;

    CloseCommandDialog();
    commandProcessor_.Execute(line);
    AddCommandToHistory(line);
    persistenceStatus_.clear();
    return true;
}

std::string Application::BuildDialogCommand() const {
    if (!commandDialog_) return {};
    const auto& dialog = *commandDialog_;

    if (dialog.command == "color" && dialog.fields.size() == 3) {
        const auto& target = dialog.fields[0].value;
        const auto& foreground = dialog.fields[1].value;
        const auto& background = dialog.fields[2].value;
        if (target == "default") {
            return "color " + QuoteArgument(foreground) +
                   " bg " + QuoteArgument(background);
        }
        return "color " + QuoteArgument(target) + " " +
               QuoteArgument(foreground) + " bg " +
               QuoteArgument(background);
    }

    std::string result = dialog.command;
    for (const auto& field : dialog.fields) {
        result += " " + QuoteArgument(field.value);
    }
    return result;
}

std::vector<std::string> Application::TaskIdOptions() const {
    std::vector<std::string> result;
    for (const auto& task : taskManager_.GetTasks()) result.push_back(task.GetId());
    return result;
}

std::vector<std::string> Application::ColorNameOptions() const {
    std::vector<std::string> result{
        "black", "red", "green", "yellow", "blue", "magenta", "cyan",
        "white", "brightBlack", "brightRed", "brightGreen", "brightYellow",
        "brightBlue", "brightMagenta", "brightCyan", "brightWhite"
    };
    for (const auto& [name, color] : taskManager_.GetDefinedColors()) {
        (void)color;
        result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::string Application::QuoteArgument(const std::string& value) {
    return QuoteIfNeeded(value);
}

void Application::Render() {
    SetNormalUiAttributes();
    erase();
    SetNormalUiAttributes();

    const auto suggestions = commandDialog_
        ? std::vector<Suggestion>{}
        : BuildSuggestions();
    if (!suggestions.empty() && selectedSuggestion_ >= suggestions.size()) {
        selectedSuggestion_ = 0;
    }

    RenderHeader();
    RenderTasks();
    if (!commandDialog_) RenderSuggestions(suggestions);
    RenderStatus();
    RenderCommandLine();
    if (commandDialog_) RenderCommandDialog();
    refresh();
}

void Application::RenderHeader() {
    SetNormalUiAttributes();
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)height;

    const std::string title = commandDialog_
        ? "Sentinel - Productivity Tracker | COMMAND ARGUMENT WINDOW"
        : "Sentinel - Productivity Tracker | " + taskManager_.GetJsonFile() +
          " | autoSave " + std::to_string(taskManager_.GetAutoSaveInterval().count()) + "s";
    mvaddnstr(0, 0, title.c_str(), std::max(0, width - 1));
    if (width > 1) mvhline(1, 0, ACS_HLINE, width - 1);
}

std::vector<Application::Suggestion> Application::BuildSuggestions() const {
    std::vector<Suggestion> suggestions;
    if (commandBuffer_.empty()) return suggestions;

    const auto separator = commandBuffer_.find_first_of(" \t");
    const std::string command = commandBuffer_.substr(0, separator);
    if (separator == std::string::npos) {
        struct RankedCommand {
            int score;
            CommandDefinition definition;
        };
        std::vector<RankedCommand> ranked;
        for (const auto& definition : Commands) {
            const auto score = FuzzySearch::Score(command, definition.name);
            if (score) ranked.push_back({*score, definition});
        }
        std::sort(
            ranked.begin(), ranked.end(),
            [](const auto& left, const auto& right) {
                return left.score != right.score
                    ? left.score > right.score
                    : left.definition.name < right.definition.name;
            }
        );
        for (std::size_t index = 0;
             index < ranked.size() && index < MaxSuggestions;
             ++index) {
            suggestions.push_back({
                std::string(ranked[index].definition.name) + "  -  " +
                    std::string(ranked[index].definition.description),
                std::string(ranked[index].definition.name) + " "
            });
        }
        return suggestions;
    }

    if (!TakesTaskArgument(command)) return suggestions;
    const std::string query = UnquotePartial(commandBuffer_.substr(separator + 1));
    std::vector<std::size_t> indices;
    if (query.empty()) {
        for (std::size_t index = 0; index < taskManager_.GetTasks().size(); ++index) {
            indices.push_back(index);
        }
    } else {
        indices = taskManager_.Search(query);
    }

    for (const auto index : indices) {
        if (suggestions.size() >= MaxSuggestions) break;
        const Task* task = taskManager_.GetTask(index);
        if (!task) continue;
        std::ostringstream label;
        label << task->GetId() << "  -  " << task->GetName();
        if (task->IsCompleted()) label << "  [completed]";
        else if (task->IsRunning()) label << "  [running]";
        suggestions.push_back({
            label.str(),
            command + " " + QuoteIfNeeded(task->GetId())
        });
    }
    return suggestions;
}

std::vector<std::size_t> Application::VisibleTaskIndices() const {
    if (commandProcessor_.GetSearchResults()) return *commandProcessor_.GetSearchResults();
    std::vector<std::size_t> result;
    for (std::size_t index = 0; index < taskManager_.GetTasks().size(); ++index) {
        result.push_back(index);
    }
    return result;
}

int Application::SuggestionStartRow(std::size_t count) const {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)width;
    return std::max(2, height - 2 - static_cast<int>(count));
}

void Application::RenderTasks() {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);

    const auto suggestions = commandDialog_
        ? std::vector<Suggestion>{}
        : BuildSuggestions();
    const int lastRow = SuggestionStartRow(suggestions.size());
    const std::size_t capacity = static_cast<std::size_t>(std::max(0, lastRow - 2));
    int row = 2;

    if (!commandProcessor_.GetInfoLines().empty()) {
        SetNormalUiAttributes();
        const auto& lines = commandProcessor_.GetInfoLines();
        const std::size_t maxOffset = lines.size() > capacity ? lines.size() - capacity : 0;
        ContentScrollOffset = std::min(ContentScrollOffset, maxOffset);
        for (std::size_t index = ContentScrollOffset;
             index < lines.size() && row < lastRow;
             ++index) {
            mvaddnstr(row++, 0, lines[index].c_str(), std::max(0, width - 1));
        }
        return;
    }

    const auto indices = VisibleTaskIndices();
    const std::size_t maxOffset = indices.size() > capacity ? indices.size() - capacity : 0;
    ContentScrollOffset = std::min(ContentScrollOffset, maxOffset);

    std::size_t slot = 0;
    for (std::size_t position = ContentScrollOffset;
         position < indices.size() && row < lastRow;
         ++position) {
        const auto index = indices[position];
        const Task* task = taskManager_.GetTask(index);
        if (!task) continue;

        const char* state = task->IsCompleted()
            ? "[x]"
            : task->IsRunning() ? "   " : "[ ]";
        const std::string idField = task->GetId() + " " + state;
        const std::string nameField = task->GetName();
        const std::string timeField =
            FormatDuration(task->GetElapsedTime()) + "  " +
            task->GetCompletionDateString();

        const int rightEdge = std::max(0, width - 1);
        const int nameColumn = std::clamp(width / 4, 8, std::max(8, rightEdge));
        const int timeWidth = static_cast<int>(timeField.size());
        const int desiredTimeColumn = rightEdge - timeWidth;
        const int minimumTimeColumn = nameColumn + 8;
        const int timeColumn = std::max(minimumTimeColumn, desiredTimeColumn);
        const int idWidth = std::max(0, nameColumn - 2);
        const int nameWidth = std::max(
            0,
            std::min(timeColumn - nameColumn - 2, rightEdge - nameColumn)
        );
        const int actualTimeColumn = std::min(
            timeColumn,
            std::max(0, rightEdge - timeWidth)
        );
        const int actualTimeWidth = std::max(0, rightEdge - actualTimeColumn);

        short pair = task->IsRunning() ? RunningTaskPair : TaskColorPair;
        if (!task->IsRunning() && has_colors() && task->HasCustomColor()) {
            const short candidate = static_cast<short>(FirstPerTaskPair + slot);
            if (candidate > 0 && candidate < COLOR_PAIRS &&
                InitializeColorPair(
                    candidate,
                    *task->GetForegroundColor(),
                    *task->GetBackgroundColor()
                )) {
                pair = candidate;
            }
        }

        if (has_colors()) attr_set(A_NORMAL, pair, nullptr);
        else attr_set(task->IsRunning() ? A_REVERSE : A_NORMAL, 0, nullptr);
        move(row, 0);
        clrtoeol();
        DrawClippedField(row, 0, idWidth, idField);
        DrawClippedField(row, nameColumn, nameWidth, nameField);
        DrawClippedField(row, actualTimeColumn, actualTimeWidth, timeField);
        ++row;
        SetNormalUiAttributes();
        ++slot;
    }
    SetNormalUiAttributes();
}

void Application::RenderSuggestions(
    const std::vector<Suggestion>& suggestions
) {
    if (suggestions.empty()) return;
    SetNormalUiAttributes();

    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)height;
    const int start = SuggestionStartRow(suggestions.size());

    for (std::size_t index = 0; index < suggestions.size(); ++index) {
        const bool selected = index == selectedSuggestion_;
        if (selected) attron(A_REVERSE);
        const std::string line =
            (selected ? "> " : "  ") + suggestions[index].label;
        mvaddnstr(
            start + static_cast<int>(index),
            0,
            line.c_str(),
            std::max(0, width - 1)
        );
        if (selected) attroff(A_REVERSE);
    }
}

void Application::RenderStatus() {
    SetNormalUiAttributes();
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    if (height < 2) return;

    std::string status = commandProcessor_.GetStatusMessage();
    if (!persistenceStatus_.empty()) {
        status += status.empty() ? "" : " | ";
        status += persistenceStatus_;
    }
    if (commandDialog_) {
        status += status.empty() ? "" : " | ";
        status += "Tab next | Shift+Tab previous | Enter choose | F2 submit | Esc cancel";
    } else if (!BuildSuggestions().empty()) {
        status += status.empty() ? "" : " | ";
        status += "Tab complete | Down suggestions | Up history | Left/Right cursor | click select | wheel scroll";
    } else if (!commandHistory_.empty()) {
        status += status.empty() ? "" : " | ";
        status += "Up: previous command | Left/Right: cursor | Mouse wheel: scroll items";
    } else {
        status += status.empty() ? "" : " | ";
        status += "Mouse wheel: scroll items";
    }
    mvaddnstr(height - 2, 0, status.c_str(), std::max(0, width - 1));
}

void Application::RenderCommandLine() {
    SetNormalUiAttributes();
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    if (height < 1 || width < 1) return;

    const int row = height - 1;
    move(row, 0);
    clrtoeol();
    if (commandDialog_) {
        curs_set(0);
        const std::string line =
            "> [argument window: " + commandDialog_->command + "]";
        mvaddnstr(row, 0, line.c_str(), std::max(0, width - 1));
        return;
    }

    curs_set(1);
    mvaddnstr(row, 0, "> ", std::max(0, width - 1));
    if (width > 2) mvaddnstr(row, 2, commandBuffer_.c_str(), width - 3);
    move(row, std::min(width - 1, static_cast<int>(cursorPosition_) + 2));
}

void Application::RenderCommandDialog() {
    if (!commandDialog_) return;
    SetNormalUiAttributes();

    auto& dialog = *commandDialog_;
    const auto rect = CalculateDialogRect(dialog.fields.size());
    for (int y = rect.top; y < rect.top + rect.height; ++y) {
        move(y, rect.left);
        for (int x = 0; x < rect.width; ++x) addch(' ');
    }

    mvhline(rect.top, rect.left + 1, ACS_HLINE, rect.width - 2);
    mvhline(rect.top + rect.height - 1, rect.left + 1, ACS_HLINE, rect.width - 2);
    mvvline(rect.top + 1, rect.left, ACS_VLINE, rect.height - 2);
    mvvline(rect.top + 1, rect.left + rect.width - 1, ACS_VLINE, rect.height - 2);
    mvaddch(rect.top, rect.left, ACS_ULCORNER);
    mvaddch(rect.top, rect.left + rect.width - 1, ACS_URCORNER);
    mvaddch(rect.top + rect.height - 1, rect.left, ACS_LLCORNER);
    mvaddch(rect.top + rect.height - 1, rect.left + rect.width - 1, ACS_LRCORNER);

    DrawClippedField(rect.top + 1, rect.left + 3, rect.width - 6, dialog.title);
    DrawClippedField(
        rect.top + 2,
        rect.left + 3,
        rect.width - 6,
        "Fill arguments manually. CLI syntax remains supported."
    );

    const int labelWidth = std::min(18, std::max(10, rect.width / 4));
    const int inputColumn = rect.left + 3 + labelWidth;
    const int inputWidth = std::max(10, rect.width - labelWidth - 7);

    for (std::size_t index = 0; index < dialog.fields.size(); ++index) {
        auto& field = dialog.fields[index];
        const int row = DialogFieldRow(rect, index);
        DrawClippedField(row, rect.left + 3, labelWidth - 1, field.label + ":");
        if (dialog.focusedControl == index) attron(A_REVERSE);
        const std::string value =
            field.value.empty() && field.kind == DialogFieldKind::DropList
                ? "(no options)"
                : field.value;
        DrawClippedField(
            row,
            inputColumn,
            inputWidth,
            "[ " + value +
                (field.kind == DialogFieldKind::DropList ? "  v" : "") +
                " ]"
        );
        if (dialog.focusedControl == index) attroff(A_REVERSE);
    }

    const std::size_t submit = dialog.fields.size();
    const std::size_t cancel = submit + 1;
    const int buttonRow = rect.top + rect.height - 3;
    const int submitColumn = rect.left + rect.width / 2 - 14;
    const int cancelColumn = rect.left + rect.width / 2 + 3;

    if (dialog.focusedControl == submit) attron(A_REVERSE);
    DrawClippedField(buttonRow, submitColumn, 12, "[ Submit ]");
    if (dialog.focusedControl == submit) attroff(A_REVERSE);
    if (dialog.focusedControl == cancel) attron(A_REVERSE);
    DrawClippedField(buttonRow, cancelColumn, 12, "[ Cancel ]");
    if (dialog.focusedControl == cancel) attroff(A_REVERSE);

    DrawClippedField(
        rect.top + rect.height - 2,
        rect.left + 3,
        rect.width - 6,
        dialog.validationMessage
    );

    if (dialog.focusedControl < dialog.fields.size() &&
        dialog.fields[dialog.focusedControl].kind == DialogFieldKind::TextInput) {
        curs_set(1);
        move(
            DialogFieldRow(rect, dialog.focusedControl),
            std::min(
                inputColumn + inputWidth - 2,
                inputColumn + 2 + static_cast<int>(
                    dialog.fields[dialog.focusedControl].cursor
                )
            )
        );
    } else {
        curs_set(0);
    }
}

std::string Application::FormatDuration(std::chrono::seconds duration) {
    const auto total = duration.count();
    std::ostringstream output;
    output << std::setfill('0')
           << std::setw(2) << total / 3600 << ':'
           << std::setw(2) << (total % 3600) / 60 << ':'
           << std::setw(2) << total % 60;
    return output.str();
}
