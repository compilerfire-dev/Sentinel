#include "TaskTree.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace {

bool Contains(const std::vector<VisibleTreeNode>& nodes, const std::string& id) {
    return std::any_of(nodes.begin(), nodes.end(), [&](const auto& item) {
        return item.node && item.node->id == id;
    });
}

int Fail(const std::string& message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    TaskTree tree;
    std::string error;

    if (!tree.AddNode(NodeKind::Folder, "work", "root", "Work", error)) return Fail(error);
    if (!tree.AddNode(NodeKind::Folder, "archive", "root", "Archive", error)) return Fail(error);
    if (!tree.AddNode(NodeKind::Folder, "graphics", "work", "Graphics", error)) return Fail(error);
    if (!tree.AddNode(NodeKind::Task, "opengl", "graphics", "Study OpenGL", error)) return Fail(error);

    if (!tree.RenameNode("opengl", "Modern OpenGL", error)) return Fail(error);
    if (!tree.GetNode("opengl") || tree.GetNode("opengl")->name != "Modern OpenGL") {
        return Fail("rename did not update the node display name");
    }

    if (!tree.MoveNode("opengl", "archive", error)) return Fail(error);
    if (!tree.GetNode("opengl") || tree.GetNode("opengl")->parentId != "archive") {
        return Fail("move did not update the task parent");
    }

    // graphics is still a descendant of work, so moving work under graphics
    // must be rejected as a cycle.
    if (tree.MoveNode("work", "graphics", error)) {
        return Fail("cycle-producing move unexpectedly succeeded");
    }

    if (!tree.SetCollapsed("work", true, error)) return Fail(error);
    const auto hidden = tree.FlattenVisible();
    if (!Contains(hidden, "work") || Contains(hidden, "graphics")) {
        return Fail("collapsed folder did not hide descendants");
    }

    const auto full = tree.Flatten();
    if (!Contains(full, "graphics") || !Contains(full, "opengl")) {
        return Fail("full traversal lost nodes while a folder was collapsed");
    }

    if (!tree.ExpandAncestors("graphics", error)) return Fail(error);
    if (!Contains(tree.FlattenVisible(), "graphics")) {
        return Fail("ExpandAncestors did not reveal a hidden descendant");
    }

    if (!tree.ToggleCollapsed("work", error)) return Fail(error);
    if (!tree.GetNode("work") || !tree.GetNode("work")->collapsed) {
        return Fail("toggle did not collapse the folder");
    }
    if (!tree.ToggleCollapsed("work", error)) return Fail(error);
    if (tree.GetNode("work")->collapsed) {
        return Fail("second toggle did not expand the folder");
    }

    if (!tree.MoveNode("graphics", "root", error)) return Fail(error);
    if (!tree.GetNode("graphics") || !tree.GetNode("graphics")->parentId.empty()) {
        return Fail("moving a folder to root failed");
    }

    if (tree.SetCollapsed("opengl", true, error)) {
        return Fail("task node was incorrectly allowed to collapse");
    }

    return 0;
}
