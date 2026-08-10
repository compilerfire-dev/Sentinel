#include "EditorBuffer.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

int Fail(const std::string& message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    EditorBuffer buffer;
    if (buffer.LineCount() != 1 || !buffer.Line(0).empty()) {
        return Fail("new buffer should contain one empty line");
    }

    buffer.SetText("Hello\nWorld\n", true);
    if (!buffer.Modified()) return Fail("SetText should mark the buffer modified");
    if (buffer.LineCount() != 3 || buffer.Text() != "Hello\nWorld\n") {
        return Fail("SetText/Text did not preserve multiline GTK text");
    }

    buffer.NewEmpty();
    const std::string hello = "Hello";
    for (std::size_t index = 0; index < hello.size(); ++index) {
        if (!buffer.InsertCharacter(0, index, hello[index])) {
            return Fail("character insertion failed");
        }
    }

    if (!buffer.SplitLine(0, 5)) return Fail("line split failed");
    const std::string world = "World";
    for (std::size_t index = 0; index < world.size(); ++index) {
        if (!buffer.InsertCharacter(1, index, world[index])) {
            return Fail("second-line insertion failed");
        }
    }

    if (buffer.LineCount() != 2 || buffer.Line(0) != "Hello" || buffer.Line(1) != "World") {
        return Fail("split buffer contents are incorrect");
    }

    std::size_t row = 1;
    std::size_t column = 0;
    if (!buffer.Backspace(row, column)) return Fail("line join via backspace failed");
    if (row != 0 || column != 5 || buffer.LineCount() != 1 || buffer.Line(0) != "HelloWorld") {
        return Fail("backspace did not join lines correctly");
    }

    if (!buffer.SplitLine(0, 5)) return Fail("second split failed");
    row = 0;
    column = 5;
    if (!buffer.DeleteForward(row, column)) return Fail("forward delete join failed");
    if (buffer.LineCount() != 1 || buffer.Line(0) != "HelloWorld") {
        return Fail("forward delete did not join lines correctly");
    }

    const auto path = std::filesystem::temp_directory_path() /
        ("sentinel-editor-buffer-" + std::to_string(static_cast<long long>(::getpid())) + ".txt");

    std::string error;
    if (!buffer.SaveAs(path, error)) return Fail(error);
    if (buffer.Modified()) return Fail("successful save should clear modified state");

    EditorBuffer loaded;
    if (!loaded.Load(path, error)) return Fail(error);
    if (loaded.LineCount() != 1 || loaded.Line(0) != "HelloWorld" || loaded.Text() != "HelloWorld") {
        return Fail("saved buffer did not load back correctly");
    }

    if (!loaded.DeleteLine(0)) return Fail("delete line failed");
    if (loaded.LineCount() != 1 || !loaded.Line(0).empty()) {
        return Fail("deleting the only line should leave one empty line");
    }

    std::filesystem::remove(path);
    return 0;
}
