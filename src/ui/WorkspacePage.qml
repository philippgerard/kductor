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

    Component.onCompleted: {
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

    actions: [
        Kirigami.Action {
            text: i18n("Add Agent")
            icon.name: "list-add-symbolic"
            onTriggered: addAgent()
        },
        Kirigami.Action {
            text: showingDiff ? i18n("Agents") : i18n("Diff")
            icon.name: showingDiff ? "system-run-symbolic" : "vcs-diff"
            shortcut: "Ctrl+D"
            onTriggered: {
                showingDiff = !showingDiff;
                if (showingDiff) diffViewer.reload();
            }
        }
    ]

    // Placeholder when no agents and not showing diff
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

    // Agents view
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

    // Diff view (inline, not a separate page)
    DiffViewerPage {
        id: diffViewer
        anchors.fill: parent
        visible: showingDiff
        worktreePath: workspacePage.worktreePath
        sourceBranch: workspacePage.sourceBranch
        workspaceName: workspacePage.workspaceName
    }

    // Header info bar
    header: QQC2.ToolBar {
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
            QQC2.Label {
                text: i18np("%1 agent", "%1 agents", agents.length)
                opacity: 0.7
            }
        }
    }
}
