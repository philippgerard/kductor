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

    property var currentAgentId: null
    property var agentIds: []

    actions: [
        Kirigami.Action {
            text: i18n("Add Agent")
            icon.name: "list-add-symbolic"
            onTriggered: addAgentSheet.open()
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
            onTriggered: addAgentSheet.open()
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
                    onClicked: {
                        currentAgentId = modelData;
                    }
                }
            }
        }

        // Agent panel
        AgentPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: currentAgentId !== null
            agentId: currentAgentId || ""
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

    Kirigami.Dialog {
        id: addAgentSheet
        title: i18n("Add Agent")
        preferredWidth: Kirigami.Units.gridUnit * 25

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            Kirigami.FormLayout {
                QQC2.TextArea {
                    id: promptField
                    Kirigami.FormData.label: i18n("Prompt:")
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 8
                    placeholderText: i18n("What should the agent work on?")
                    wrapMode: TextEdit.Wrap
                }

                QQC2.ComboBox {
                    id: modelCombo
                    Kirigami.FormData.label: i18n("Model:")
                    model: ["sonnet", "opus", "haiku"]
                    currentIndex: 0
                }
            }
        }

        footer: QQC2.DialogButtonBox {
            QQC2.Button {
                text: i18n("Cancel")
                QQC2.DialogButtonBox.buttonRole: QQC2.DialogButtonBox.RejectRole
                onClicked: addAgentSheet.close()
            }
            QQC2.Button {
                text: i18n("Start Agent")
                icon.name: "media-playback-start-symbolic"
                QQC2.DialogButtonBox.buttonRole: QQC2.DialogButtonBox.AcceptRole
                enabled: promptField.text.length > 0
                onClicked: {
                    let agentId = AgentManager.spawnAgent(
                        workspacePage.workspaceId,
                        workspacePage.worktreePath,
                        promptField.text,
                        modelCombo.currentText
                    );
                    agentIds = [...agentIds, agentId];
                    currentAgentId = agentId;
                    addAgentSheet.close();
                    promptField.text = "";
                }
            }
        }
    }
}
