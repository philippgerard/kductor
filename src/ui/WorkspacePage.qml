import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kductor

Kirigami.Page {
    id: workspacePage

    required property string workspaceId
    required property string workspaceName
    required property string worktreePath
    required property string branchName
    required property string sourceBranch
    required property string repoPath

    title: workspaceName

    property string currentAgentId: ""
    property var agents: []
    property int nextAgentNumber: 1
    property bool showingDiff: false
    property bool operationBusy: false
    property string forge: ""
    property string webUrl: ""

    Component.onCompleted: {
        forge = WorktreeManager.detectForge(worktreePath);
        webUrl = WorktreeManager.remoteWebUrl(worktreePath);
        let existing = AgentManager.agentsForWorkspace(workspaceId);
        if (existing.length > 0) {
            let restored = [];
            let maxNum = 0;
            for (let i = 0; i < existing.length; i++) {
                let name = AgentManager.agentName(existing[i]) || i18n("Agent %1", i + 1);
                restored.push({id: existing[i], name: name});
                let match = name.match(/(\d+)$/);
                if (match) maxNum = Math.max(maxNum, parseInt(match[1]));
            }
            agents = restored;
            nextAgentNumber = maxNum + 1;
            currentAgentId = existing[0];
        }
    }

    function addAgent() {
        showingDiff = false;
        let name = i18n("Agent %1", nextAgentNumber);
        let agentId = AgentManager.createAgent(workspacePage.workspaceId, name);
        nextAgentNumber++;
        agents = [...agents, {id: agentId, name: name}];
        currentAgentId = agentId;
    }

    function removeAgent(agentId) {
        AgentManager.removeAgent(agentId);
        agents = agents.filter(a => a.id !== agentId);
        if (currentAgentId === agentId) {
            currentAgentId = agents.length > 0 ? agents[0].id : "";
        }
    }

    function agentIndex(agentId) {
        for (let i = 0; i < agents.length; i++) {
            if (agents[i].id === agentId) return i;
        }
        return 0;
    }

    // --- Operation result handling ---
    Connections {
        target: WorktreeManager
        function onOperationStarted(op) {
            operationBusy = true;
            statusMessage.visible = false;
        }
        function onOperationSucceeded(op, result) {
            operationBusy = false;
            if (op === "archive") {
                let repo = repoPath;
                WorkspaceModel.remove(workspaceId);
                applicationWindow().selectedWorkspaceId = "";
                applicationWindow().selectedWorkspaceName = "";
                applicationWindow().loadRepoOverview(repo);
                applicationWindow().showPassiveNotification(i18n("Workspace archived."));
                return;
            }
            let msg = "";
            if (op === "push") msg = i18n("Branch pushed.");
            else if (op === "pr") msg = result || i18n("Pull request created.");
            else if (op === "merge-pr") { msg = i18n("Pull request merged."); WorkspaceModel.updateStatus(workspaceId, 2); }
            else if (op === "merge") { msg = i18n("Merged into %1.", sourceBranch); WorkspaceModel.updateStatus(workspaceId, 2); }
            applicationWindow().showPassiveNotification(msg, 4000);
        }
        function onOperationFailed(op, error) {
            operationBusy = false;
            statusMessage.type = Kirigami.MessageType.Error;
            statusMessage.text = error;
            statusMessage.visible = true;
        }
    }

    // --- Actions ---
    // Keyboard shortcuts
    Shortcut { sequence: "Ctrl+D"; onActivated: { showingDiff = !showingDiff; if (showingDiff) diffViewer.reload(); } }

    // --- Header ---
    header: ColumnLayout {
        spacing: 0

        QQC2.ToolBar {
            Layout.fillWidth: true
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: "vcs-branch"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                }
                QQC2.Label {
                    text: branchName
                    elide: Text.ElideRight
                }
                Kirigami.Separator {
                    Layout.fillHeight: true
                }
                Kirigami.Icon {
                    source: "folder-symbolic"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                }
                QQC2.Label {
                    text: repoPath
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Kirigami.Separator { Layout.fillHeight: true }

                QQC2.ToolButton {
                    icon.name: "list-add-symbolic"
                    text: i18n("Agent")
                    display: QQC2.AbstractButton.TextBesideIcon
                    onClicked: addAgent()
                }
                QQC2.ToolButton {
                    icon.name: showingDiff ? "system-run-symbolic" : "vcs-diff"
                    text: showingDiff ? i18n("Agents") : i18n("Diff")
                    display: QQC2.AbstractButton.TextBesideIcon
                    onClicked: { showingDiff = !showingDiff; if (showingDiff) diffViewer.reload(); }
                }
                QQC2.ToolButton {
                    icon.name: "vcs-push"
                    text: forge === "github" ? i18n("Push & PR") : i18n("Push")
                    display: QQC2.AbstractButton.TextBesideIcon
                    enabled: !operationBusy
                    onClicked: forge === "github" ? pushThenPR() : pushThenOpenWeb()
                }
                QQC2.ToolButton {
                    id: mergeButton
                    icon.name: "vcs-merge"
                    text: i18n("Merge")
                    display: QQC2.AbstractButton.TextBesideIcon
                    enabled: !operationBusy
                    onClicked: mergeMenu.popup(mergeButton, 0, mergeButton.height)

                    QQC2.Menu {
                        id: mergeMenu
                        QQC2.MenuItem {
                            text: i18n("Merge PR (remote)")
                            icon.name: "vcs-merge"
                            visible: forge === "github"
                            onTriggered: mergePrDialog.open()
                        }
                        QQC2.MenuItem {
                            text: i18n("Merge locally to %1", sourceBranch)
                            icon.name: "vcs-merge"
                            onTriggered: mergeLocalDialog.open()
                        }
                        QQC2.MenuSeparator {}
                        QQC2.MenuItem {
                            text: i18n("Archive workspace")
                            icon.name: "archive-remove"
                            onTriggered: archiveDialog.open()
                        }
                    }
                }
            }
        }

        // Status message banner
        Kirigami.InlineMessage {
            id: statusMessage
            Layout.fillWidth: true
            visible: false
            showCloseButton: true
            onVisibleChanged: {
                if (!visible) text = "";
            }
        }
    }

    // --- Placeholder ---
    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        visible: agents.length === 0 && !showingDiff
        width: parent.width - (Kirigami.Units.largeSpacing * 4)
        icon.name: "system-run-symbolic"
        text: i18n("No agents yet")
        explanation: i18n("Add an agent to start working on this workspace.")
        helpfulAction: Kirigami.Action {
            text: i18n("Add Agent")
            icon.name: "list-add-symbolic"
            onTriggered: addAgent()
        }
    }

    // --- Agents view ---
    ColumnLayout {
        anchors.fill: parent
        visible: agents.length > 0 && !showingDiff
        spacing: 0

        QQC2.TabBar {
            id: agentTabBar
            Layout.fillWidth: true

            Repeater {
                model: agents
                QQC2.TabButton {
                    required property var modelData
                    text: modelData.name
                    checked: currentAgentId === modelData.id
                    onClicked: currentAgentId = modelData.id
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            border.width: 1
            border.color: Kirigami.ColorUtils.linearInterpolation(
                Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.1)

            StackLayout {
                anchors.fill: parent
                anchors.margins: 1
                currentIndex: agentIndex(currentAgentId)

                Repeater {
                    model: agents
                    AgentPanel {
                        required property var modelData
                        agentId: modelData.id
                        workingDir: worktreePath
                        onCloseRequested: function(id) { removeAgent(id) }
                    }
                }
            }
        }
    }

    // --- Diff view ---
    DiffViewerPage {
        id: diffViewer
        anchors.fill: parent
        visible: showingDiff
        worktreePath: workspacePage.worktreePath
        sourceBranch: workspacePage.sourceBranch
        workspaceName: workspacePage.workspaceName
    }

    // --- Dialogs ---

    function pushThenOpenWeb() {
        function onDone(op, result) {
            if (op !== "push") return;
            WorktreeManager.operationSucceeded.disconnect(onDone);
            WorktreeManager.operationFailed.disconnect(onFail);
            // Open web UI to create PR
            let prUrl = webUrl + "/compare/" + sourceBranch + "..." + branchName;
            Qt.openUrlExternally(prUrl);
        }
        function onFail(op, error) {
            if (op !== "push") return;
            WorktreeManager.operationSucceeded.disconnect(onDone);
            WorktreeManager.operationFailed.disconnect(onFail);
        }
        WorktreeManager.operationSucceeded.connect(onDone);
        WorktreeManager.operationFailed.connect(onFail);
        WorktreeManager.pushBranch(worktreePath);
    }

    function pushThenPR() {
        function onPushDone(op, result) {
            if (op !== "push") return;
            WorktreeManager.operationSucceeded.disconnect(onPushDone);
            WorktreeManager.operationFailed.disconnect(onPushFail);
            WorktreeManager.createPullRequest(worktreePath, workspaceName, "");
        }
        function onPushFail(op, error) {
            if (op !== "push") return;
            WorktreeManager.operationSucceeded.disconnect(onPushDone);
            WorktreeManager.operationFailed.disconnect(onPushFail);
        }
        WorktreeManager.operationSucceeded.connect(onPushDone);
        WorktreeManager.operationFailed.connect(onPushFail);
        WorktreeManager.pushBranch(worktreePath);
    }

    // Simple confirmations — PromptDialog (native KDE message boxes)

    Kirigami.PromptDialog {
        id: mergePrDialog
        title: i18n("Merge Pull Request")
        subtitle: i18n("Merge and delete the remote branch on GitHub?")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Merge PR")
                icon.name: "vcs-merge"
                onTriggered: {
                    mergePrDialog.close();
                    WorktreeManager.mergePullRequest(worktreePath);
                }
            }
        ]
    }

    Kirigami.PromptDialog {
        id: mergeLocalDialog
        title: i18n("Merge Locally")
        subtitle: i18n("Merge '%1' into '%2'?", branchName, sourceBranch)
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Merge")
                icon.name: "vcs-merge"
                onTriggered: {
                    mergeLocalDialog.close();
                    WorktreeManager.mergeToSource(repoPath, branchName, sourceBranch);
                }
            }
        ]
    }

    Kirigami.PromptDialog {
        id: archiveDialog
        title: i18n("Archive Workspace")
        subtitle: i18n("Remove worktree and agent sessions? The branch '%1' will remain.", branchName)
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Archive")
                icon.name: "archive-remove"
                onTriggered: {
                    archiveDialog.close();
                    for (let a of agents) AgentManager.removeAgent(a.id);
                    WorktreeManager.archiveWorkspace(worktreePath, repoPath);
                }
            }
        ]
    }
}
