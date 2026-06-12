# Kductor

*Your code, conducted.*

A KDE-native AI agent orchestrator. Run multiple Claude Code agents in parallel across isolated git worktrees, review diffs, and manage the full PR/merge lifecycle — all from a single desktop app.

Inspired by [Conductor](https://www.conductor.build) for macOS.

![Built with](https://img.shields.io/badge/Built%20with-KDE%20Frameworks%206-blue)
![License](https://img.shields.io/badge/License-MIT-green)

## Features

**Agent orchestration**
- Spawn multiple Claude Code agents per workspace, each working in an isolated git worktree
- Stream agent output in real-time with markdown rendering (with selectable text)
- Collapsible thinking blocks, grouped tool actions, and inspectable tool results
- Per-agent model selection (Fable 5, Opus, Sonnet, Haiku)
- Multi-line prompt input with prompt history (↑/↓) and per-session cost display
- Multi-turn conversations resumed by session id, persisted across app restarts
- Configurable permission mode (full access ↔ ask/plan) and extra CLI flags
- Stop a running agent; prompts are never silently dropped while one is busy

**Diff & code review**
- Built-in diff viewer powered by libgit2, computed off the UI thread
- Three modes: All changes, Committed only, Pending (staged + unstaged)
- New (untracked) files render full content; theme-aware, selectable, copyable
- Color-coded unified diff with line numbers and hunk headers
- Toggle between agent view and diff view with Ctrl+D

**PR & merge workflow**
- Push branches to remote
- One-click PR creation (GitHub via `gh`, Gitea/others via browser)
- Auto-generated PR title and description from commit history using Claude Haiku
- Live PR status tracking with colored status indicator
- Auto-detect when PR is merged remotely and pull the source branch
- Prompt to archive workspace after PR is merged
- Local merge with auto-commit of uncommitted worktree changes
- Workspace archival with worktree cleanup
- Forge auto-detection (GitHub, Gitea, GitLab)

**KDE integration**
- System tray with live agent count and window toggle; "Continue in Background"
- KDE notifications on completion (with workspace context and click-to-focus)
- Settings page (default model, permission mode, extra flags, max agents, tray, notifications, CLI recheck)
- Native Kirigami UI with Breeze theme, full keyboard accessibility, and dark-mode-correct colors
- Window geometry and sidebar width persist across restarts

**Workspace management**
- Sidebar with repos and nested workspaces (Conductor-style), resizable
- Repo overview page with stats
- Add repos without creating workspaces (with clear errors for non-Git folders)
- Per-repo workspace creation with branch selection
- Commit with a message, open the worktree folder, copy branch/path/PR URL
- Keyboard shortcuts: Ctrl+T new agent, Ctrl+PgUp/PgDn switch agents, Ctrl+D diff

## Requirements

- Qt 6.6+
- KDE Frameworks 6 (Kirigami, KI18n, KCoreAddons, KConfig, KDBusAddons, KNotifications, KStatusNotifierItem, KCrash, KWindowSystem, KIconThemes)
- kirigami-addons
- libgit2
- Extra CMake Modules (ECM)
- [Claude Code CLI](https://claude.ai/claude-code) (`claude` in PATH or `~/.local/bin`)
- `gh` CLI (optional, for GitHub PR creation)

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

Or launch from your KDE application menu.

## Usage

1. **Add a repository** — Click "Add repository" in the sidebar and select a local git repo
2. **Create a workspace** — Click the + button next to a repo to create an isolated worktree
3. **Add an agent** — Click "Agent" in the workspace toolbar, then type a prompt
4. **Review changes** — Toggle the diff viewer with Ctrl+D or the "Diff" button
5. **Push & merge** — Push to remote, create a PR, or merge locally

## Architecture

```
src/
├── core/
│   ├── gitmanager.cpp        # libgit2 wrapper (repos, worktrees, diffs)
│   ├── worktreemanager.cpp   # Workspace lifecycle, push/PR/merge ops
│   ├── agentprocess.cpp      # QProcess wrapper, Claude stream-json parser
│   ├── agentmanager.cpp      # Multi-agent pool, settings, persistence
│   ├── agentoutputmodel.cpp  # Ring-buffered output model
│   ├── workspace.cpp         # Data model + JSON persistence
│   └── workspacemodel.cpp    # QAbstractListModel for QML
├── ui/
│   ├── Main.qml              # App shell, sidebar, content loader
│   ├── WorkspacePage.qml     # Agent tabs, toolbar, diff toggle
│   ├── AgentPanel.qml        # Streaming output, command bar, model picker
│   ├── DiffViewerPage.qml    # Diff viewer with mode switcher
│   ├── RepoOverviewPage.qml  # Repo stats and workspace list
│   ├── SettingsPage.qml      # FormCard settings
│   └── components/           # StatusBadge, StreamingTextArea, CommandBar, CopyButton
└── main.cpp                  # Entry point, tray, notifications
```

## Why "Kductor"?

**K** (KDE) + Con**ductor** — a conductor orchestrates musicians, Kductor orchestrates AI agents. Also a nod to [Conductor](https://www.conductor.build), the macOS app that inspired it.

## License

MIT License. See [LICENSE](LICENSE).
