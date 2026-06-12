import QtQuick
import QtCore
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kductor

Kirigami.ApplicationWindow {
    id: root

    title: selectedWorkspaceId ? selectedWorkspaceName + " — Kductor" : i18n("Kductor")
    minimumWidth: Kirigami.Units.gridUnit * 50
    minimumHeight: Kirigami.Units.gridUnit * 30
    width: Kirigami.Units.gridUnit * 70
    height: Kirigami.Units.gridUnit * 40

    property string selectedWorkspaceId: ""
    property string selectedWorkspaceName: ""

    // Persist window geometry and sidebar width across restarts.
    property real sidebarWidth: Kirigami.Units.gridUnit * 14
    Settings {
        id: windowSettings
        category: "window"
        property alias width: root.width
        property alias height: root.height
        property alias sidebarWidth: root.sidebarWidth
    }

    function loadRepoOverview(repoPath) {
        selectedWorkspaceId = "";
        selectedWorkspaceName = "";
        workspaceLoader.setSource(Qt.resolvedUrl("RepoOverviewPage.qml"), {
            repoPath: repoPath
        });
    }

    onClosing: function(close) {
        if (AgentManager.activeCount > 0) {
            close.accepted = false;
            quitDialog.open();
        }
    }

    Kirigami.PromptDialog {
        id: quitDialog
        title: i18n("Agents Running")
        subtitle: i18np("%1 agent is still running. Quit anyway?", "%1 agents are still running. Quit anyway?", AgentManager.activeCount)
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Continue in Background")
                icon.name: "window-minimize-symbolic"
                visible: AgentManager.showInTray
                onTriggered: {
                    quitDialog.close();
                    root.hide(); // keep agents running; restore from the tray
                }
            },
            Kirigami.Action {
                text: i18n("Stop & Quit")
                icon.name: "application-exit"
                onTriggered: {
                    quitDialog.close();
                    AgentManager.stopAll();
                    Qt.quit();
                }
            }
        ]
    }

    globalDrawer: Kirigami.GlobalDrawer {
        title: i18n("Kductor")
        titleIcon: "kductor"
        isMenu: true
        actions: [
            Kirigami.Action {
                text: i18n("Settings")
                icon.name: "configure"
                onTriggered: pageStack.pushDialogLayer(settingsComp)
            },
            Kirigami.Action {
                text: i18n("About Kductor")
                icon.name: "help-about"
                onTriggered: pageStack.pushDialogLayer(aboutComp)
            }
        ]
    }

    Component {
        id: settingsComp
        SettingsPage {}
    }
    Component {
        id: aboutComp
        AboutPage {}
    }

    // No pageStack navigation — we use a custom sidebar + content layout
    pageStack.columnView.columnResizeMode: Kirigami.ColumnView.SingleColumn
    pageStack.initialPage: Kirigami.Page {
        id: mainPage
        title: i18n("Kductor")
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0

        RowLayout {
            anchors.fill: parent
            spacing: 0

            // --- Sidebar ---
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: root.sidebarWidth
                Layout.minimumWidth: Kirigami.Units.gridUnit * 10
                Layout.maximumWidth: Kirigami.Units.gridUnit * 28
                // Subtle tint so the sidebar reads as distinct from the content.
                color: Kirigami.ColorUtils.linearInterpolation(
                    Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.04)

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Grouped repo → workspace list
                    QQC2.ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

                        Flickable {
                            contentWidth: availableWidth
                            contentHeight: sidebarContent.implicitHeight
                            clip: true

                            ColumnLayout {
                                id: sidebarContent
                                width: parent.width
                                spacing: 0

                                // Rebuild when model changes
                                property var repos: WorkspaceModel.count >= 0 ? WorkspaceModel.uniqueRepoPaths() : []

                                Repeater {
                                    model: sidebarContent.repos

                                    ColumnLayout {
                                        id: repoSection
                                        Layout.fillWidth: true
                                        spacing: 0

                                        required property string modelData
                                        readonly property string repoName: modelData.split("/").pop()
                                        property var workspaces: WorkspaceModel.count >= 0 ? WorkspaceModel.workspacesForRepo(modelData) : []

                                        // Repo header
                                        QQC2.ItemDelegate {
                                            Layout.fillWidth: true
                                            Accessible.name: i18n("Repository %1", repoSection.repoName)
                                            contentItem: RowLayout {
                                                spacing: Kirigami.Units.smallSpacing
                                                Kirigami.Icon {
                                                    source: "folder-git-symbolic"
                                                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                                                    Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                                                    opacity: 0.7
                                                }
                                                QQC2.Label {
                                                    text: repoSection.repoName
                                                    font.bold: true
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                                QQC2.ToolButton {
                                                    icon.name: "list-add-symbolic"
                                                    Accessible.name: i18n("New workspace in %1", repoSection.repoName)
                                                    onClicked: createSheet.openForRepo(repoSection.modelData)
                                                    QQC2.ToolTip.text: i18n("New workspace")
                                                    QQC2.ToolTip.visible: hovered
                                                }
                                            }
                                            onClicked: loadRepoOverview(repoSection.modelData)
                                        }

                                        // Workspaces under this repo
                                        Repeater {
                                            model: repoSection.workspaces

                                            QQC2.ItemDelegate {
                                                id: wsItem
                                                Layout.fillWidth: true
                                                highlighted: modelData.id === selectedWorkspaceId
                                                leftPadding: Kirigami.Units.largeSpacing * 2

                                                readonly property bool hasAgents: AgentManager.activeCount >= 0 && AgentManager.agentsForWorkspace(modelData.id).length > 0
                                                readonly property bool agentsRunning: AgentManager.activeCount >= 0 && AgentManager.workspaceAgentStatus(modelData.id) === 2
                                                readonly property string stateText: agentsRunning ? i18n("agent working")
                                                    : hasAgents ? i18n("agents idle") : i18n("no agents")

                                                Accessible.name: i18n("%1 — %2", modelData.name, stateText)
                                                QQC2.ToolTip.text: stateText
                                                QQC2.ToolTip.visible: hovered && hasAgents

                                                contentItem: RowLayout {
                                                    spacing: Kirigami.Units.smallSpacing

                                                    Rectangle {
                                                        id: wsDot
                                                        width: Kirigami.Units.smallSpacing * 2
                                                        height: width
                                                        radius: width / 2

                                                        color: {
                                                            if (wsItem.agentsRunning) return Kirigami.Theme.focusColor;
                                                            if (wsItem.hasAgents) return Kirigami.Theme.positiveTextColor;
                                                            return Kirigami.Theme.disabledTextColor;
                                                        }
                                                        opacity: (!wsItem.hasAgents) ? 0.4 : 1.0

                                                        SequentialAnimation on opacity {
                                                            running: wsItem.agentsRunning && Kirigami.Units.longDuration > 1
                                                            loops: Animation.Infinite
                                                            NumberAnimation { to: 0.3; duration: Kirigami.Units.longDuration * 4; easing.type: Easing.InOutQuad }
                                                            NumberAnimation { to: 1.0; duration: Kirigami.Units.longDuration * 4; easing.type: Easing.InOutQuad }
                                                        }
                                                    }

                                                    QQC2.Label {
                                                        text: modelData.name
                                                        elide: Text.ElideRight
                                                        Layout.fillWidth: true
                                                        font.bold: modelData.id === selectedWorkspaceId
                                                    }
                                                }

                                                onClicked: {
                                                    selectedWorkspaceId = modelData.id;
                                                    selectedWorkspaceName = modelData.name;
                                                    workspaceLoader.setSource(Qt.resolvedUrl("WorkspacePage.qml"), {
                                                        workspaceId: modelData.id,
                                                        workspaceName: modelData.name,
                                                        worktreePath: modelData.worktreePath,
                                                        branchName: modelData.branchName,
                                                        sourceBranch: modelData.sourceBranch,
                                                        repoPath: modelData.repoPath
                                                    });
                                                }

                                                // F2 renames the focused workspace (keyboard alternative
                                                // to the right-click menu).
                                                Keys.onPressed: function(event) {
                                                    if (event.key === Qt.Key_F2) {
                                                        renameDialog.workspaceId = modelData.id;
                                                        renameDialog.currentName = modelData.name;
                                                        renameDialog.open();
                                                        event.accepted = true;
                                                    }
                                                }

                                                QQC2.Menu {
                                                    id: wsContextMenu
                                                    QQC2.MenuItem {
                                                        text: i18n("Rename…")
                                                        icon.name: "edit-rename"
                                                        onTriggered: {
                                                            renameDialog.workspaceId = modelData.id;
                                                            renameDialog.currentName = modelData.name;
                                                            renameDialog.open();
                                                        }
                                                    }
                                                }

                                                TapHandler {
                                                    acceptedButtons: Qt.RightButton
                                                    onTapped: wsContextMenu.popup()
                                                }
                                            }
                                        }
                                    }
                                }

                                // Add repository button at bottom
                                QQC2.ItemDelegate {
                                    Layout.fillWidth: true
                                    Layout.topMargin: Kirigami.Units.smallSpacing
                                    Accessible.name: i18n("Add repository")
                                    contentItem: RowLayout {
                                        spacing: Kirigami.Units.smallSpacing
                                        Kirigami.Icon {
                                            source: "list-add-symbolic"
                                            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                                            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                                            opacity: 0.5
                                        }
                                        QQC2.Label {
                                            text: i18n("Add repository")
                                            opacity: 0.5
                                            Layout.fillWidth: true
                                        }
                                    }
                                    onClicked: addRepoDialog.open()
                                }
                            }
                        }
                    }
                }
            }

            // Vertical separator, draggable to resize the sidebar
            Item {
                Layout.fillHeight: true
                Layout.preferredWidth: 1

                Kirigami.Separator { anchors.fill: parent }

                MouseArea {
                    anchors.fill: parent
                    anchors.leftMargin: -Kirigami.Units.smallSpacing
                    anchors.rightMargin: -Kirigami.Units.smallSpacing
                    cursorShape: Qt.SizeHorCursor
                    onPositionChanged: function(mouse) {
                        if (!pressed) return;
                        let gx = mapToItem(null, mouse.x, 0).x;
                        root.sidebarWidth = Math.max(Kirigami.Units.gridUnit * 10,
                            Math.min(Kirigami.Units.gridUnit * 28, gx));
                    }
                }
            }

            // --- Main content ---
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // Onboarding: warn when the claude CLI is missing, before the
                // user has invested in creating repos/workspaces/agents.
                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.smallSpacing
                    type: Kirigami.MessageType.Warning
                    visible: !AgentManager.claudeAvailable
                    text: i18n("Claude Code CLI not found. Install it with: npm install -g @anthropic-ai/claude-code")
                    actions: [
                        Kirigami.Action {
                            text: i18n("Check again")
                            icon.name: "view-refresh"
                            onTriggered: AgentManager.redetectClaude()
                        }
                    ]
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                // Empty state
                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    visible: workspaceLoader.source == ""
                    width: parent.width - Kirigami.Units.largeSpacing * 4
                    icon.name: "kductor"
                    text: sidebarContent.repos.length === 0
                        ? i18n("Get started")
                        : i18n("No workspace selected")
                    explanation: sidebarContent.repos.length === 0
                        ? i18n("Add a Git repository to start orchestrating AI agents.")
                        : i18n("Select a workspace from the sidebar or add one with the + button.")
                    helpfulAction: Kirigami.Action {
                        text: sidebarContent.repos.length === 0 ? i18n("Add Repository") : ""
                        icon.name: "list-add-symbolic"
                        visible: sidebarContent.repos.length === 0
                        onTriggered: addRepoDialog.open()
                    }
                }

                // Loaded workspace
                Loader {
                    id: workspaceLoader
                    anchors.fill: parent
                }
                }
            }
        }
    }

    WorkspaceCreateSheet {
        id: createSheet
        onAccepted: {
            // Find and select the newly created workspace
            let ws = WorkspaceModel.getById(WorkspaceModel.lastAddedId);
            if (ws && ws.id) {
                selectedWorkspaceId = ws.id;
                selectedWorkspaceName = ws.name;
                workspaceLoader.setSource(Qt.resolvedUrl("WorkspacePage.qml"), {
                    workspaceId: ws.id,
                    workspaceName: ws.name,
                    worktreePath: ws.worktreePath,
                    branchName: ws.branchName,
                    sourceBranch: ws.sourceBranch,
                    repoPath: ws.repoPath
                });
            }
        }
    }

    FolderDialog {
        id: addRepoDialog
        title: i18n("Select Git Repository")
        onAccepted: {
            let path = decodeURIComponent(selectedFolder.toString().replace("file://", ""));
            if (GitManager.openRepository(path)) {
                WorkspaceModel.addRepo(path);
            } else {
                applicationWindow().showPassiveNotification(
                    i18n("Not a Git repository: %1", path), 5000);
            }
        }
    }

    Kirigami.PromptDialog {
        id: renameDialog

        property string workspaceId: ""
        property string currentName: ""

        title: i18n("Rename Workspace")

        onOpened: {
            renameField.text = currentName;
            renameField.selectAll();
            renameField.forceActiveFocus();
        }

        Kirigami.ActionTextField {
            id: renameField
            placeholderText: i18n("Workspace name")
            onAccepted: renameDialog.accept()
        }

        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel

        onAccepted: {
            let newName = renameField.text.trim();
            if (newName.length > 0 && newName !== currentName) {
                WorkspaceModel.rename(workspaceId, newName);
                if (selectedWorkspaceId === workspaceId) {
                    selectedWorkspaceName = newName;
                    if (workspaceLoader.item) {
                        workspaceLoader.item.workspaceName = newName;
                    }
                }
            }
        }
    }
}
