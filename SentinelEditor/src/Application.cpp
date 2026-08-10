#include "Application.hpp"

#include <algorithm>
#include <cctype>
#include <clocale>
#include <ncurses.h>
#include <sstream>

namespace {

constexpr std::size_t MouseWheelStep = 3;

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

bool IsWordCharacter(char value) {
    const unsigned char c = static_cast<unsigned char>(value);
    return std::isalnum(c) != 0 || value == '_';
}

} // namespace

int Application::Run(int argc, char** argv) {
    std::setlocale(LC_ALL, "");

    if (argc > 1 && argv[1] && *argv[1]) {
        std::string error;
        if (!buffer_.Load(argv[1], error)) status_ = error;
        else status_ = "Opened " + buffer_.Path().string();
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS, nullptr);
    curs_set(1);

    while (running_) {
        Render();
        HandleInput(getch());
    }

    endwin();
    return 0;
}

void Application::HandleInput(int key) {
    if (key == KEY_MOUSE) {
        HandleMouse();
        return;
    }

    switch (mode_) {
        case Mode::Normal: HandleNormalInput(key); break;
        case Mode::Insert: HandleInsertInput(key); break;
        case Mode::Command: HandleCommandInput(key); break;
        case Mode::Search: HandleSearchInput(key); break;
    }

    ClampCursor();
    EnsureCursorVisible();
}

void Application::HandleNormalInput(int key) {
    if (pendingDelete_) {
        pendingDelete_ = false;
        if (key == 'd') {
            const std::size_t oldRow = cursorRow_;
            if (buffer_.DeleteLine(cursorRow_)) {
                cursorRow_ = std::min(oldRow, buffer_.LineCount() - 1);
                cursorColumn_ = 0;
                preferredColumn_ = 0;
                status_ = "Deleted line";
            }
            return;
        }
    }

    if (pendingGoto_) {
        pendingGoto_ = false;
        if (key == 'g') {
            cursorRow_ = 0;
            cursorColumn_ = 0;
            preferredColumn_ = 0;
            return;
        }
    }

    switch (key) {
        case 27:
            status_.clear();
            break;
        case KEY_LEFT:
        case 'h':
            MoveHorizontal(-1);
            break;
        case KEY_RIGHT:
        case 'l':
            MoveHorizontal(1);
            break;
        case KEY_UP:
        case 'k':
            MoveVertical(-1);
            break;
        case KEY_DOWN:
        case 'j':
            MoveVertical(1);
            break;
        case KEY_PPAGE:
        case 2: // Ctrl-B
            MovePage(-1);
            break;
        case KEY_NPAGE:
        case 6: // Ctrl-F
            MovePage(1);
            break;
        case '0':
        case KEY_HOME:
            cursorColumn_ = 0;
            preferredColumn_ = 0;
            break;
        case '$':
        case KEY_END: {
            const auto& line = buffer_.Line(cursorRow_);
            cursorColumn_ = line.empty() ? 0 : line.size() - 1;
            preferredColumn_ = cursorColumn_;
            break;
        }
        case 'w':
            MoveWordForward();
            break;
        case 'b':
            MoveWordBackward();
            break;
        case 'g':
            pendingGoto_ = true;
            status_ = "g";
            break;
        case 'G':
            cursorRow_ = buffer_.LineCount() - 1;
            cursorColumn_ = 0;
            preferredColumn_ = 0;
            break;
        case 'i':
            EnterInsertMode();
            break;
        case 'a': {
            const auto& line = buffer_.Line(cursorRow_);
            cursorColumn_ = line.empty() ? 0 : std::min(line.size(), cursorColumn_ + 1);
            EnterInsertMode();
            break;
        }
        case 'I':
            cursorColumn_ = 0;
            EnterInsertMode();
            break;
        case 'A':
            cursorColumn_ = buffer_.Line(cursorRow_).size();
            EnterInsertMode();
            break;
        case 'o':
            if (buffer_.InsertBlankLineAfter(cursorRow_)) {
                ++cursorRow_;
                cursorColumn_ = 0;
                preferredColumn_ = 0;
                EnterInsertMode();
            }
            break;
        case 'O':
            if (buffer_.InsertBlankLineBefore(cursorRow_)) {
                cursorColumn_ = 0;
                preferredColumn_ = 0;
                EnterInsertMode();
            }
            break;
        case 'x':
            if (buffer_.EraseCharacter(cursorRow_, cursorColumn_)) {
                status_ = "Deleted character";
            }
            break;
        case 'd':
            pendingDelete_ = true;
            status_ = "d";
            break;
        case ':':
            EnterCommandMode();
            break;
        case '/':
            EnterSearchMode();
            break;
        case 'n':
            if (lastSearch_.empty()) status_ = "No previous search";
            else if (!FindNext(lastSearch_, true)) status_ = "Pattern not found: " + lastSearch_;
            break;
        default:
            break;
    }
}

void Application::HandleInsertInput(int key) {
    if (key == 27) {
        EnterNormalMode();
        return;
    }

    if (key == KEY_LEFT) {
        if (cursorColumn_ > 0) --cursorColumn_;
        preferredColumn_ = cursorColumn_;
        return;
    }
    if (key == KEY_RIGHT) {
        cursorColumn_ = std::min(buffer_.Line(cursorRow_).size(), cursorColumn_ + 1);
        preferredColumn_ = cursorColumn_;
        return;
    }
    if (key == KEY_UP) {
        MoveVertical(-1);
        return;
    }
    if (key == KEY_DOWN) {
        MoveVertical(1);
        return;
    }
    if (key == KEY_HOME) {
        cursorColumn_ = 0;
        preferredColumn_ = 0;
        return;
    }
    if (key == KEY_END) {
        cursorColumn_ = buffer_.Line(cursorRow_).size();
        preferredColumn_ = cursorColumn_;
        return;
    }

    if (key == '\n' || key == KEY_ENTER) {
        if (buffer_.SplitLine(cursorRow_, cursorColumn_)) {
            ++cursorRow_;
            cursorColumn_ = 0;
            preferredColumn_ = 0;
        }
        return;
    }

    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        buffer_.Backspace(cursorRow_, cursorColumn_);
        preferredColumn_ = cursorColumn_;
        return;
    }

#ifdef KEY_DC
    if (key == KEY_DC) {
        buffer_.DeleteForward(cursorRow_, cursorColumn_);
        preferredColumn_ = cursorColumn_;
        return;
    }
#endif

    if (key >= 32 && key <= 255) {
        if (buffer_.InsertCharacter(cursorRow_, cursorColumn_, static_cast<char>(key))) {
            ++cursorColumn_;
            preferredColumn_ = cursorColumn_;
        }
    }
}

void Application::HandleCommandInput(int key) {
    if (key == 27) {
        EnterNormalMode();
        return;
    }
    if (key == '\n' || key == KEY_ENTER) {
        const std::string command = commandBuffer_;
        commandBuffer_.clear();
        mode_ = Mode::Normal;
        ExecuteCommand(command);
        return;
    }
    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (!commandBuffer_.empty()) commandBuffer_.pop_back();
        return;
    }
    if (key >= 32 && key <= 255) commandBuffer_.push_back(static_cast<char>(key));
}

void Application::HandleSearchInput(int key) {
    if (key == 27) {
        EnterNormalMode();
        return;
    }
    if (key == '\n' || key == KEY_ENTER) {
        const std::string query = searchBuffer_;
        searchBuffer_.clear();
        mode_ = Mode::Normal;
        if (query.empty()) return;
        lastSearch_ = query;
        if (!FindNext(query, true)) status_ = "Pattern not found: " + query;
        return;
    }
    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (!searchBuffer_.empty()) searchBuffer_.pop_back();
        return;
    }
    if (key >= 32 && key <= 255) searchBuffer_.push_back(static_cast<char>(key));
}

void Application::HandleMouse() {
    MEVENT event{};
    if (getmouse(&event) != OK) return;

    if (IsWheelUp(event.bstate)) {
        topLine_ = topLine_ > MouseWheelStep ? topLine_ - MouseWheelStep : 0;
        return;
    }
    if (IsWheelDown(event.bstate)) {
        const std::size_t maximum = buffer_.LineCount() > TextRows()
            ? buffer_.LineCount() - TextRows()
            : 0;
        topLine_ = std::min(maximum, topLine_ + MouseWheelStep);
        return;
    }

    if ((event.bstate & BUTTON1_CLICKED) == 0 &&
        (event.bstate & BUTTON1_PRESSED) == 0) {
        return;
    }

    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    if (event.y < 0 || event.y >= height - 2) return;

    const std::size_t row = topLine_ + static_cast<std::size_t>(event.y);
    if (row >= buffer_.LineCount()) return;

    cursorRow_ = row;
    const int textX = event.x - static_cast<int>(GutterWidth());
    cursorColumn_ = leftColumn_ + static_cast<std::size_t>(std::max(0, textX));
    preferredColumn_ = cursorColumn_;
    ClampCursor();
}

void Application::EnterNormalMode() {
    mode_ = Mode::Normal;
    commandBuffer_.clear();
    searchBuffer_.clear();
    pendingDelete_ = false;
    pendingGoto_ = false;

    const auto& line = buffer_.Line(cursorRow_);
    if (!line.empty() && cursorColumn_ == line.size()) --cursorColumn_;
    ClampCursor();
}

void Application::EnterInsertMode() {
    mode_ = Mode::Insert;
    pendingDelete_ = false;
    pendingGoto_ = false;
    status_.clear();
    ClampCursor();
}

void Application::EnterCommandMode() {
    mode_ = Mode::Command;
    commandBuffer_.clear();
    pendingDelete_ = false;
    pendingGoto_ = false;
}

void Application::EnterSearchMode() {
    mode_ = Mode::Search;
    searchBuffer_.clear();
    pendingDelete_ = false;
    pendingGoto_ = false;
}

void Application::ExecuteCommand(const std::string& rawCommand) {
    const std::string command = Trim(rawCommand);
    if (command.empty()) return;

    const auto space = command.find_first_of(" \t");
    const std::string operation = space == std::string::npos
        ? command
        : command.substr(0, space);
    const std::string argument = space == std::string::npos
        ? std::string{}
        : Unquote(Trim(command.substr(space + 1)));

    if (operation == "w") {
        SaveFile(argument);
        return;
    }
    if (operation == "q") {
        if (buffer_.Modified()) {
            status_ = "No write since last change (use :q! to discard)";
            return;
        }
        running_ = false;
        return;
    }
    if (operation == "q!") {
        running_ = false;
        return;
    }
    if (operation == "wq" || operation == "x") {
        if (SaveFile(argument)) running_ = false;
        return;
    }
    if (operation == "e" || operation == "e!") {
        if (argument.empty()) {
            status_ = "Usage: :e[!] <path>";
            return;
        }
        LoadFile(argument, operation == "e!");
        return;
    }
    if (operation == "new" || operation == "new!") {
        if (buffer_.Modified() && operation != "new!") {
            status_ = "No write since last change (use :new! to discard)";
            return;
        }
        buffer_.NewEmpty();
        cursorRow_ = cursorColumn_ = preferredColumn_ = topLine_ = leftColumn_ = 0;
        status_ = "New buffer";
        return;
    }
    if (operation == "help") {
        status_ = "Normal: hjkl/arrows w b 0 $ gg G i a I A o O x dd / n | Ex: :w :q :q! :wq :e :e! :new";
        return;
    }

    status_ = "Not an editor command: " + operation;
}

bool Application::LoadFile(const std::string& path, bool force) {
    if (buffer_.Modified() && !force) {
        status_ = "No write since last change (use :e! <path> to discard)";
        return false;
    }

    std::string error;
    if (!buffer_.Load(path, error)) {
        status_ = error;
        return false;
    }

    cursorRow_ = cursorColumn_ = preferredColumn_ = topLine_ = leftColumn_ = 0;
    status_ = "Opened " + buffer_.Path().string();
    return true;
}

bool Application::SaveFile(const std::string& path) {
    std::string error;
    const bool saved = path.empty()
        ? buffer_.Save(error)
        : buffer_.SaveAs(path, error);
    if (!saved) {
        status_ = error;
        return false;
    }

    status_ = "Written " + buffer_.Path().string() + " (" +
        std::to_string(buffer_.LineCount()) + " lines)";
    return true;
}

void Application::MoveHorizontal(int delta) {
    const auto& line = buffer_.Line(cursorRow_);
    if (delta < 0) {
        if (cursorColumn_ > 0) --cursorColumn_;
    } else if (!line.empty() && cursorColumn_ + 1 < line.size()) {
        ++cursorColumn_;
    }
    preferredColumn_ = cursorColumn_;
}

void Application::MoveVertical(int delta) {
    if (delta < 0) {
        if (cursorRow_ > 0) --cursorRow_;
    } else if (cursorRow_ + 1 < buffer_.LineCount()) {
        ++cursorRow_;
    }

    const auto& line = buffer_.Line(cursorRow_);
    const std::size_t maximum = mode_ == Mode::Insert
        ? line.size()
        : (line.empty() ? 0 : line.size() - 1);
    cursorColumn_ = std::min(preferredColumn_, maximum);
}

void Application::MoveWordForward() {
    std::size_t row = cursorRow_;
    std::size_t column = cursorColumn_;

    while (row < buffer_.LineCount()) {
        const auto& line = buffer_.Line(row);
        if (column < line.size()) {
            if (IsWordCharacter(line[column])) {
                while (column < line.size() && IsWordCharacter(line[column])) ++column;
            }
            while (column < line.size() && !IsWordCharacter(line[column])) ++column;
            if (column < line.size()) {
                cursorRow_ = row;
                cursorColumn_ = column;
                preferredColumn_ = column;
                return;
            }
        }
        ++row;
        column = 0;
    }
}

void Application::MoveWordBackward() {
    std::size_t row = cursorRow_;
    std::size_t column = cursorColumn_;

    while (true) {
        const auto& line = buffer_.Line(row);
        if (!line.empty()) {
            if (column >= line.size()) column = line.size() - 1;
            if (column > 0) --column;
            while (column > 0 && !IsWordCharacter(line[column])) --column;
            while (column > 0 && IsWordCharacter(line[column - 1])) --column;
            if (IsWordCharacter(line[column])) {
                cursorRow_ = row;
                cursorColumn_ = column;
                preferredColumn_ = column;
                return;
            }
        }
        if (row == 0) return;
        --row;
        column = buffer_.Line(row).size();
    }
}

void Application::MovePage(int direction) {
    const std::size_t page = std::max<std::size_t>(1, TextRows() - 1);
    if (direction < 0) {
        cursorRow_ = cursorRow_ > page ? cursorRow_ - page : 0;
    } else {
        cursorRow_ = std::min(buffer_.LineCount() - 1, cursorRow_ + page);
    }
    const auto& line = buffer_.Line(cursorRow_);
    const std::size_t maximum = line.empty() ? 0 : line.size() - 1;
    cursorColumn_ = std::min(preferredColumn_, maximum);
}

void Application::ClampCursor() {
    if (buffer_.LineCount() == 0) return;
    cursorRow_ = std::min(cursorRow_, buffer_.LineCount() - 1);
    const auto& line = buffer_.Line(cursorRow_);
    const std::size_t maximum = mode_ == Mode::Insert
        ? line.size()
        : (line.empty() ? 0 : line.size() - 1);
    cursorColumn_ = std::min(cursorColumn_, maximum);
}

void Application::EnsureCursorVisible() {
    const std::size_t rows = TextRows();
    const std::size_t columns = TextColumns();

    if (cursorRow_ < topLine_) topLine_ = cursorRow_;
    else if (cursorRow_ >= topLine_ + rows) topLine_ = cursorRow_ - rows + 1;

    if (cursorColumn_ < leftColumn_) leftColumn_ = cursorColumn_;
    else if (cursorColumn_ >= leftColumn_ + columns) {
        leftColumn_ = cursorColumn_ - columns + 1;
    }
}

bool Application::FindNext(const std::string& query, bool wrap) {
    if (query.empty()) return false;

    const std::size_t startRow = cursorRow_;
    std::size_t row = cursorRow_;
    std::size_t startColumn = std::min(cursorColumn_ + 1, buffer_.Line(row).size());

    do {
        const auto& line = buffer_.Line(row);
        const auto found = line.find(query, startColumn);
        if (found != std::string::npos) {
            cursorRow_ = row;
            cursorColumn_ = found;
            preferredColumn_ = found;
            status_ = "/" + query;
            return true;
        }

        row = (row + 1) % buffer_.LineCount();
        startColumn = 0;
        if (!wrap && row <= startRow) break;
    } while (row != startRow);

    if (wrap) {
        const auto& line = buffer_.Line(startRow);
        const auto found = line.find(query, 0);
        if (found != std::string::npos && found <= cursorColumn_) {
            cursorColumn_ = found;
            preferredColumn_ = found;
            status_ = "/" + query + " (wrapped)";
            return true;
        }
    }

    return false;
}

void Application::Render() {
    erase();
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);

    RenderBuffer(height, width);
    RenderStatusLine(std::max(0, height - 2), width);
    RenderCommandLine(std::max(0, height - 1), width);

    if (height >= 3 && mode_ != Mode::Command && mode_ != Mode::Search) {
        const int screenRow = static_cast<int>(cursorRow_ - topLine_);
        const std::size_t logicalStart = std::min(leftColumn_, buffer_.Line(cursorRow_).size());
        const std::size_t logicalEnd = std::min(cursorColumn_, buffer_.Line(cursorRow_).size());
        const std::string prefix = logicalEnd >= logicalStart
            ? buffer_.Line(cursorRow_).substr(logicalStart, logicalEnd - logicalStart)
            : std::string{};
        const int screenColumn = static_cast<int>(GutterWidth() + ExpandTabs(prefix).size());
        move(
            std::clamp(screenRow, 0, height - 3),
            std::clamp(screenColumn, 0, std::max(0, width - 1))
        );
        curs_set(1);
    }

    refresh();
}

void Application::RenderBuffer(int height, int width) {
    const int contentRows = std::max(0, height - 2);
    const int gutter = static_cast<int>(GutterWidth());
    const int textWidth = std::max(0, width - gutter);

    for (int screenRow = 0; screenRow < contentRows; ++screenRow) {
        const std::size_t bufferRow = topLine_ + static_cast<std::size_t>(screenRow);
        if (bufferRow >= buffer_.LineCount()) {
            if (width > 0) mvaddch(screenRow, 0, '~');
            continue;
        }

        std::ostringstream lineNumber;
        lineNumber.width(std::max(1, gutter - 1));
        lineNumber << std::right << bufferRow + 1 << ' ';
        const std::string number = lineNumber.str();
        if (gutter > 0) mvaddnstr(screenRow, 0, number.c_str(), gutter);

        const auto& source = buffer_.Line(bufferRow);
        const std::size_t start = std::min(leftColumn_, source.size());
        const std::string visible = ExpandTabs(source.substr(start));
        if (textWidth > 0) mvaddnstr(screenRow, gutter, visible.c_str(), textWidth);
    }
}

void Application::RenderStatusLine(int row, int width) {
    if (row < 0 || width <= 0) return;
    attron(A_REVERSE);
    move(row, 0);
    clrtoeol();

    const std::string path = buffer_.HasPath() ? buffer_.Path().string() : "[No Name]";
    std::ostringstream status;
    status << " " << ModeName() << "  " << path;
    if (buffer_.Modified()) status << " [+]";
    status << "   " << (cursorRow_ + 1) << ":" << (cursorColumn_ + 1)
           << "   " << buffer_.LineCount() << " lines";

    const std::string value = status.str();
    mvaddnstr(row, 0, value.c_str(), width);
    attroff(A_REVERSE);
}

void Application::RenderCommandLine(int row, int width) {
    if (row < 0 || width <= 0) return;
    move(row, 0);
    clrtoeol();

    if (mode_ == Mode::Command) {
        const std::string value = ":" + commandBuffer_;
        mvaddnstr(row, 0, value.c_str(), width);
        move(row, std::min(width - 1, static_cast<int>(value.size())));
        curs_set(1);
        return;
    }
    if (mode_ == Mode::Search) {
        const std::string value = "/" + searchBuffer_;
        mvaddnstr(row, 0, value.c_str(), width);
        move(row, std::min(width - 1, static_cast<int>(value.size())));
        curs_set(1);
        return;
    }

    const std::string value = status_.empty()
        ? "i insert | : command | / search | :help"
        : status_;
    mvaddnstr(row, 0, value.c_str(), width);
}

std::size_t Application::GutterWidth() const {
    return std::max<std::size_t>(4, std::to_string(buffer_.LineCount()).size() + 2);
}

std::size_t Application::TextRows() const {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)width;
    return static_cast<std::size_t>(std::max(1, height - 2));
}

std::size_t Application::TextColumns() const {
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    (void)height;
    return static_cast<std::size_t>(std::max(1, width - static_cast<int>(GutterWidth())));
}

std::string Application::ModeName() const {
    switch (mode_) {
        case Mode::Normal: return "NORMAL";
        case Mode::Insert: return "INSERT";
        case Mode::Command: return "COMMAND";
        case Mode::Search: return "SEARCH";
    }
    return "NORMAL";
}

std::string Application::Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string Application::Unquote(std::string value) {
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

std::string Application::ExpandTabs(const std::string& value, std::size_t tabWidth) {
    std::string result;
    std::size_t column = 0;
    for (const char c : value) {
        if (c == '\t') {
            const std::size_t spaces = tabWidth - (column % tabWidth);
            result.append(spaces, ' ');
            column += spaces;
        } else {
            result.push_back(c);
            ++column;
        }
    }
    return result;
}
