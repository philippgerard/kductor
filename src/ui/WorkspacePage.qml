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
    property string operationName: ""

    function operationLabel(op) {
        if (op === "push") return i18n("Pushing branch…");
        if (op === "pr") return i18n("Creating pull request…");
        if (op === "merge-pr") return i18n("Merging pull request…");
        if (op === "merge") return i18n("Merging locally…");
        if (op === "commit") return i18n("Committing changes…");
        if (op === "archive") return i18n("Archiving workspace…");
        if (op === "pull") return i18n("Updating source branch…");
        return i18n("Working…");
    }

    // Clipboard helper (QtQuick exposes copy() only through TextEdit).
    TextEdit { id: clipboardHelper; visible: false }
    function copyToClipboard(text, label) {
        clipboardHelper.text = text;
        clipboardHelper.selectAll();
        clipboardHelper.copy();
        clipboardHelper.text = "";
        applicationWindow().showPassiveNotification(label || i18n("Copied to clipboard"), 1500);
    }
    property string forge: ""
    property string webUrl: ""
    property bool remoteAvailable: false
    property string prUrl: ""
    property int prNumber: 0
    property string prState: "none"  // none, OPEN, MERGED, CLOSED
    property string prMergeable: ""
    property string prChecks: "none" // none, pending, success, failure

    Connections {
        target: WorktreeManager
        function onPrStatusChecked(url, number, state, mergeable, checks) {
            let wasPrOpen = prState === "OPEN";
            prUrl = url;
            prNumber = number;
            prState = state;
            prMergeable = mergeable;
            prChecks = checks;

            // Auto-pull when PR was merged remotely
            if (wasPrOpen && state === "MERGED") {
                WorkspaceModel.updateStatus(workspaceId, 2);
                WorktreeManager.pullSource(repoPath, sourceBranch);
                applicationWindow().showPassiveNotification(
                    i18n("PR #%1 merged. Pulling %2…", number, sourceBranch), 4000);
                cleanupAfterMergeDialog.open();
            }
        }
    }

    Timer {
        id: prPollTimer
        interval: 15000
        repeat: true
        // Don't poll the remote while the window is hidden in the tray.
        running: remoteAvailable && prState === "OPEN"
                 && (applicationWindow().visible || false)
        onTriggered: WorktreeManager.checkPrStatus(worktreePath)
    }

    // React to git operations performed by the agent (commit, push, PR, merge)
    Connections {
        target: AgentManager
        function onGitEventDetected(wsId, eventType) {
            if (wsId !== workspaceId) return;

            if (eventType === "push" || eventType === "pr-created") {
                // Agent pushed or created a PR — check status after a brief delay
                // to let the remote operation settle
                agentGitRefreshTimer.restart();
            } else if (eventType === "pr-merged") {
                WorktreeManager.checkPrStatus(worktreePath);
            } else if (eventType === "commit") {
                if (showingDiff) diffViewer.reload();
            } else if (eventType === "merge") {
                WorkspaceModel.updateStatus(workspaceId, 2);
            }
        }
    }

    // Debounce timer for agent-initiated push/PR — gives the remote a moment to settle
    Timer {
        id: agentGitRefreshTimer
        interval: 2000
        repeat: false
        onTriggered: WorktreeManager.checkPrStatus(worktreePath)
    }

    Component.onCompleted: {
        let info = WorktreeManager.remoteInfo(worktreePath);
        remoteAvailable = info.hasRemote;
        forge = info.forge;
        webUrl = info.webUrl;
        if (remoteAvailable) WorktreeManager.checkPrStatus(worktreePath);
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
            operationName = op;
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
            if (op === "commit") {
                msg = i18n("Changes committed.");
                if (showingDiff) diffViewer.reload();
            }
            else if (op === "push") msg = i18n("Branch pushed.");
            else if (op === "pr") {
                msg = result || i18n("Pull request created.");
                // Refresh PR status immediately
                WorktreeManager.checkPrStatus(worktreePath);
            }
            else if (op === "merge-pr") {
                msg = i18n("Pull request merged.");
                WorkspaceModel.updateStatus(workspaceId, 2);
                prState = "MERGED"; prNumber = 0; prUrl = "";
            }
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
    Shortcut { sequence: "Ctrl+T"; onActivated: addAgent() }
    Shortcut {
        sequence: "Ctrl+PgDown"
        onActivated: {
            if (agents.length < 2) return;
            let i = agentIndex(currentAgentId);
            currentAgentId = agents[(i + 1) % agents.length].id;
        }
    }
    Shortcut {
        sequence: "Ctrl+PgUp"
        onActivated: {
            if (agents.length < 2) return;
            let i = agentIndex(currentAgentId);
            currentAgentId = agents[(i - 1 + agents.length) % agents.length].id;
        }
    }

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
                    icon.name: "vcs-commit"
                    text: i18n("Commit")
                    display: QQC2.AbstractButton.TextBesideIcon
                    enabled: !operationBusy
                    onClicked: commitDialog.open()
                    QQC2.ToolTip.text: i18n("Commit all changes in this workspace")
                    QQC2.ToolTip.visible: hovered
                }
                Item {
                    visible: remoteAvailable && prState === "OPEN"
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: Kirigami.Units.smallSpacing * 2.5
                    implicitHeight: width
                    readonly property string prStateText: {
                        if (prChecks === "failure") return i18n("PR checks failing");
                        if (prMergeable === "CONFLICTING") return i18n("PR has conflicts");
                        if (prChecks === "pending") return i18n("PR checks running");
                        if (prChecks === "success") return i18n("PR checks passed");
                        return i18n("Pull request open");
                    }
                    Accessible.role: Accessible.Indicator
                    Accessible.name: prStateText
                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: {
                            if (prChecks === "success" && (prMergeable === "MERGEABLE" || prMergeable === "")) return Kirigami.Theme.positiveTextColor;
                            if (prChecks === "failure" || prMergeable === "CONFLICTING") return Kirigami.Theme.negativeTextColor;
                            if (prChecks === "pending") return Kirigami.Theme.neutralTextColor;
                            return Kirigami.Theme.positiveTextColor;
                        }
                    }
                    HoverHandler { id: prDotHover }
                    QQC2.ToolTip.text: prStateText
                    QQC2.ToolTip.visible: prDotHover.hovered
                }
                QQC2.ToolButton {
                    visible: remoteAvailable
                    enabled: !operationBusy
                    display: QQC2.AbstractButton.TextBesideIcon
                    icon.name: prState === "OPEN" ? "vcs-merge" : "vcs-push"
                    text: {
                        if (prState === "OPEN") return i18n("PR #%1", prNumber);
                        return forge === "github" ? i18n("Push & PR") : i18n("Push");
                    }
                    onClicked: {
                        if (prState === "OPEN" && prUrl.length > 0) {
                            Qt.openUrlExternally(prUrl);
                        } else {
                            if (forge === "github") guardedOperation("pushPR"); else guardedOperation("pushWeb");
                        }
                    }
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
                            onTriggered: guardedOperation("mergePR")
                        }
                        QQC2.MenuItem {
                            text: i18n("Merge locally to %1", sourceBranch)
                            icon.name: "vcs-merge"
                            onTriggered: guardedOperation("mergeLocal")
                        }
                    }
                }

                QQC2.ToolButton {
                    id: actionsButton
                    icon.name: "application-menu-symbolic"
                    Accessible.name: i18n("Workspace actions")
                    QQC2.ToolTip.text: i18n("Workspace actions")
                    QQC2.ToolTip.visible: hovered
                    onClicked: actionsMenu.popup(actionsButton, 0, actionsButton.height)

                    QQC2.Menu {
                        id: actionsMenu
                        QQC2.MenuItem {
                            text: i18n("Open folder")
                            icon.name: "folder-open-symbolic"
                            onTriggered: Qt.openUrlExternally("file://" + worktreePath)
                        }
                        QQC2.MenuItem {
                            text: i18n("Copy branch name")
                            icon.name: "edit-copy-symbolic"
                            onTriggered: copyToClipboard(branchName, i18n("Branch name copied"))
                        }
                        QQC2.MenuItem {
                            text: i18n("Copy worktree path")
                            icon.name: "edit-copy-symbolic"
                            onTriggered: copyToClipboard(worktreePath, i18n("Path copied"))
                        }
                        QQC2.MenuItem {
                            text: i18n("Copy PR URL")
                            icon.name: "edit-copy-symbolic"
                            enabled: prUrl.length > 0
                            visible: prState === "OPEN"
                            onTriggered: copyToClipboard(prUrl, i18n("PR URL copied"))
                        }
                        QQC2.MenuSeparator {}
                        QQC2.MenuItem {
                            text: i18n("Archive workspace…")
                            icon.name: "archive-remove"
                            onTriggered: archiveDialog.open()
                        }
                    }
                }
            }
        }

        // Operation progress banner
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
            visible: operationBusy
            spacing: Kirigami.Units.smallSpacing

            QQC2.BusyIndicator {
                running: operationBusy
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: Kirigami.Units.iconSizes.small
            }
            QQC2.Label {
                text: operationLabel(operationName)
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
            }
            Item { Layout.fillWidth: true }
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

    // --- Uncommitted changes guard ---

    // Stores which operation to run after the user decides
    property string pendingOperation: ""  // "pushWeb", "pushPR", "mergePR", "mergeLocal"

    function guardedOperation(operation) {
        if (GitManager.hasUncommittedChanges(worktreePath)) {
            pendingOperation = operation;
            uncommittedDialog.open();
        } else {
            executePendingOperation(operation);
        }
    }

    function executePendingOperation(op) {
        if (op === "pushWeb") pushThenOpenWeb();
        else if (op === "pushPR") pushThenPR();
        else if (op === "mergePR") mergePrDialog.open();
        else if (op === "mergeLocal") mergeLocalDialog.open();
    }

    Kirigami.PromptDialog {
        id: uncommittedDialog
        title: i18n("Uncommitted Changes")
        subtitle: i18n("This workspace has uncommitted changes that will not be included. Commit them first?")
        standardButtons: Kirigami.Dialog.Cancel

        onOpened: {
            guardCommitField.text = "";
            guardCommitField.forceActiveFocus();
        }

        QQC2.TextField {
            id: guardCommitField
            placeholderText: i18n("Commit message (optional)")
        }

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Commit && Continue")
                icon.name: "vcs-commit"
                onTriggered: {
                    uncommittedDialog.close();
                    let op = pendingOperation;
                    let msg = guardCommitField.text;
                    function onCommitDone(finishedOp, result) {
                        if (finishedOp !== "commit") return;
                        WorktreeManager.operationSucceeded.disconnect(onCommitDone);
                        WorktreeManager.operationFailed.disconnect(onCommitFail);
                        executePendingOperation(op);
                    }
                    function onCommitFail(finishedOp, error) {
                        if (finishedOp !== "commit") return;
                        WorktreeManager.operationSucceeded.disconnect(onCommitDone);
                        WorktreeManager.operationFailed.disconnect(onCommitFail);
                    }
                    WorktreeManager.operationSucceeded.connect(onCommitDone);
                    WorktreeManager.operationFailed.connect(onCommitFail);
                    WorktreeManager.commitAll(worktreePath, msg);
                }
            },
            Kirigami.Action {
                text: i18n("Continue Anyway")
                icon.name: "dialog-warning-symbolic"
                onTriggered: {
                    uncommittedDialog.close();
                    executePendingOperation(pendingOperation);
                }
            }
        ]
    }

    // Standalone commit (from the toolbar Commit button)
    Kirigami.PromptDialog {
        id: commitDialog
        title: i18n("Commit Changes")
        subtitle: i18n("Stage and commit all changes in this workspace.")
        standardButtons: Kirigami.Dialog.Cancel

        onOpened: {
            commitField.text = "";
            commitField.forceActiveFocus();
        }

        QQC2.TextField {
            id: commitField
            placeholderText: i18n("Commit message (optional)")
            onAccepted: commitDialog.doCommit()
        }

        function doCommit() {
            commitDialog.close();
            WorktreeManager.commitAll(worktreePath, commitField.text);
        }

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Commit")
                icon.name: "vcs-commit"
                onTriggered: commitDialog.doCommit()
            }
        ]
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

    Kirigami.Dialog {
        id: archiveDialog
        title: i18n("Archive Workspace")
        preferredWidth: Kirigami.Units.gridUnit * 24
        standardButtons: Kirigami.Dialog.Cancel

        property bool hasUncommitted: false
        onOpened: hasUncommitted = GitManager.hasUncommittedChanges(worktreePath)

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: i18n("Remove worktree and agent sessions?")
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                type: Kirigami.MessageType.Warning
                visible: archiveDialog.hasUncommitted
                text: i18n("This workspace has uncommitted changes that will be permanently lost.")
            }

            QQC2.CheckBox {
                id: deleteBranchCheck
                text: i18n("Also delete branch '%1'", branchName)
                checked: false
            }
        }

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Archive")
                icon.name: "archive-remove"
                onTriggered: {
                    archiveDialog.close();
                    for (let a of agents) AgentManager.removeAgent(a.id);
                    WorktreeManager.archiveWorkspace(worktreePath, repoPath, deleteBranchCheck.checked ? branchName : "");
                }
            }
        ]
    }

    Kirigami.PromptDialog {
        id: cleanupAfterMergeDialog
        title: i18n("PR Merged")
        subtitle: i18n("The pull request was merged and the remote branch deleted. Archive this workspace and clean up the local branch?")
        standardButtons: Kirigami.Dialog.No
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Archive & Delete Branch")
                icon.name: "archive-remove"
                onTriggered: {
                    cleanupAfterMergeDialog.close();
                    for (let a of agents) AgentManager.removeAgent(a.id);
                    WorktreeManager.archiveWorkspace(worktreePath, repoPath, branchName);
                }
            },
            Kirigami.Action {
                text: i18n("Archive Only")
                icon.name: "archive-remove"
                onTriggered: {
                    cleanupAfterMergeDialog.close();
                    for (let a of agents) AgentManager.removeAgent(a.id);
                    WorktreeManager.archiveWorkspace(worktreePath, repoPath, "");
                }
            }
        ]
    }
}
