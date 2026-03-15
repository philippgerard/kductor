import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kductor

Kirigami.ScrollablePage {
    id: dashboardPage

    title: i18n("Dashboard")

    actions: [
        Kirigami.Action {
            text: i18n("New Workspace")
            icon.name: "list-add-symbolic"
            shortcut: "Ctrl+N"
            onTriggered: createSheet.open()
        }
    ]

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        visible: WorkspaceModel.count === 0
        width: parent.width - (Kirigami.Units.largeSpacing * 4)
        icon.name: "kductor"
        text: i18n("No workspaces yet")
        explanation: i18n("Create a workspace to start orchestrating AI agents on your code.")
        helpfulAction: Kirigami.Action {
            text: i18n("New Workspace")
            icon.name: "list-add-symbolic"
            onTriggered: createSheet.open()
        }
    }

    GridLayout {
        visible: WorkspaceModel.count > 0
        columns: Math.max(1, Math.floor(dashboardPage.width / (Kirigami.Units.gridUnit * 20)))
        rowSpacing: Kirigami.Units.largeSpacing
        columnSpacing: Kirigami.Units.largeSpacing

        Repeater {
            model: WorkspaceModel
            delegate: WorkspaceCard {
                required property int index
                required property string workspaceId
                required property string name
                required property string repoPath
                required property string branchName
                required property int status
                required property int agentCount

                Layout.fillWidth: true

                workspaceName: name
                repositoryPath: repoPath
                branch: branchName
                workspaceStatus: status
                agents: AgentManager.agentsForWorkspace(workspaceId).length

                onClicked: {
                    let wsData = WorkspaceModel.get(index);
                    applicationWindow().pageStack.push(Qt.resolvedUrl("WorkspacePage.qml"), {
                        workspaceId: wsData.id,
                        workspaceName: wsData.name,
                        worktreePath: wsData.worktreePath,
                        branchName: wsData.branchName,
                        sourceBranch: wsData.sourceBranch,
                        repoPath: wsData.repoPath
                    });
                }
            }
        }
    }

    WorkspaceCreateSheet {
        id: createSheet
        onAccepted: WorkspaceModel.refresh()
    }
}
