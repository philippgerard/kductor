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
    required property string repoPath

    title: workspaceName

    property string currentAgentId: ""
    property var agentIds: []

    actions: [
        Kirigami.Action {
            text: i18n("Add Agent")
            icon.name: "list-add-symbolic"
            onTriggered: {
                let agentId = AgentManager.createAgent(workspacePage.workspaceId);
                agentIds = [...agentIds, agentId];
                currentAgentId = agentId;
            }
        }
    ]

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        visible: agentIds.length === 0
        width: parent.width - (Kirigami.Units.largeSpacing * 4)
        icon.name: "system-run-symbolic"
        text: i18n("No agents yet")
        explanation: i18n("Add an agent to start working on this workspace.")
        helpfulAction: Kirigami.Action {
            text: i18n("Add Agent")
            icon.name: "list-add-symbolic"
            onTriggered: {
                let agentId = AgentManager.createAgent(workspacePage.workspaceId);
                agentIds = [...agentIds, agentId];
                currentAgentId = agentId;
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        visible: agentIds.length > 0
        spacing: 0

        // Agent tabs
        QQC2.TabBar {
            id: agentTabBar
            Layout.fillWidth: true
            visible: agentIds.length > 1

            Repeater {
                model: agentIds
                QQC2.TabButton {
                    required property string modelData
                    required property int index
                    text: i18n("Agent %1", index + 1)
                    checked: currentAgentId === modelData
                    onClicked: currentAgentId = modelData
                }
            }
        }

        // Agent panel
        AgentPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: currentAgentId.length > 0
            agentId: currentAgentId
            workingDir: worktreePath
        }
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
                text: i18np("%1 agent", "%1 agents", agentIds.length)
                opacity: 0.7
            }
        }
    }
}
