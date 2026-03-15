# Kductor Development Guide

## Build

```bash
cmake -B build/ --install-prefix ~/.local
cmake --build build/
cmake --install build/
```

## Run

```bash
kductor
```

Kill previous instances first if needed: `pkill -f "bin/kductor"`

## Architecture

- **C++ backend** in `src/core/` — git operations (libgit2), agent process management (QProcess), workspace persistence (JSON)
- **QML frontend** in `src/ui/` — Kirigami pages and components
- **Build system** — CMake with ECM (Extra CMake Modules), KDE Frameworks 6

## Key classes

- `GitManager` — libgit2 RAII wrapper for repos, worktrees, branches, diffs
- `WorktreeManager` — workspace lifecycle, push/PR/merge operations (async via QProcess)
- `AgentProcess` — spawns `claude` CLI, parses stream-json output, emits typed signals
- `AgentManager` — multi-agent pool, settings persistence, output model ownership
- `AgentOutputModel` — QAbstractListModel with ring buffer for streaming output
- `WorkspaceModel` — workspace list model with JSON persistence

## QML structure

- `Main.qml` — app shell with sidebar (repo/workspace tree) and content loader
- `WorkspacePage.qml` — agent tabs, toolbar actions, diff toggle
- `AgentPanel.qml` — streaming output, command bar, model picker, context usage
- `DiffViewerPage.qml` — libgit2 diff with All/Committed/Pending modes
- `RepoOverviewPage.qml` — repo stats and workspace list
- `StreamingTextArea.qml` — output renderer with collapsible thinking/actions

## Dependencies

Qt 6.6+, KDE Frameworks 6 (Kirigami, I18n, CoreAddons, Config, DBusAddons, Notifications, StatusNotifierItem, Crash, WindowSystem, IconThemes), kirigami-addons, libgit2, ECM

## Testing

Requires `claude` CLI installed (`~/.local/bin/claude` or in PATH).

## Code style

- C++20, Qt naming conventions
- QML: Kirigami components, `QQC2` namespace for Qt Quick Controls
- Strings: `QStringLiteral()` for C++, `i18n()` for user-visible QML strings
