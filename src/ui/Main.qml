import QtQuick
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
                Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                Layout.minimumWidth: Kirigami.Units.gridUnit * 10
                color: Kirigami.Theme.backgroundColor

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
                                                    implicitWidth: Kirigami.Units.iconSizes.medium
                                                    implicitHeight: Kirigami.Units.iconSizes.medium
                                                    onClicked: createSheet.openForRepo(repoSection.modelData)
                                                    QQC2.ToolTip.text: i18n("New workspace")
                                                    QQC2.ToolTip.visible: hovered
                                                }
                                            }
                                        }

                                        // Workspaces under this repo
                                        Repeater {
                                            model: repoSection.workspaces

                                            QQC2.ItemDelegate {
                                                id: wsItem
                                                Layout.fillWidth: true
                                                highlighted: modelData.id === selectedWorkspaceId
                                                leftPadding: Kirigami.Units.largeSpacing * 2

                                                contentItem: RowLayout {
                                                    spacing: Kirigami.Units.smallSpacing

                                                    Rectangle {
                                                        id: wsDot
                                                        width: Kirigami.Units.smallSpacing * 2
                                                        height: width
                                                        radius: width / 2

                                                        readonly property bool hasAgents: AgentManager.activeCount >= 0 && AgentManager.agentsForWorkspace(modelData.id).length > 0
                                                        readonly property bool agentsRunning: AgentManager.activeCount >= 0 && AgentManager.workspaceAgentStatus(modelData.id) === 2

                                                        color: {
                                                            if (agentsRunning) return Kirigami.Theme.highlightedTextColor;
                                                            if (hasAgents) return Kirigami.Theme.positiveTextColor;
                                                            return Kirigami.Theme.disabledTextColor;
                                                        }
                                                        opacity: (!hasAgents) ? 0.4 : 1.0

                                                        SequentialAnimation on opacity {
                                                            running: wsDot.agentsRunning
                                                            loops: Animation.Infinite
                                                            NumberAnimation { to: 0.3; duration: 800; easing.type: Easing.InOutQuad }
                                                            NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
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
                                            }
                                        }
                                    }
                                }

                                // Add repository button at bottom
                                QQC2.ItemDelegate {
                                    Layout.fillWidth: true
                                    Layout.topMargin: Kirigami.Units.smallSpacing
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

            // Vertical separator
            Kirigami.Separator {
                Layout.fillHeight: true
            }

            // --- Main content ---
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Empty state
                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    visible: !workspaceLoader.item
                    width: parent.width - Kirigami.Units.largeSpacing * 4
                    icon.name: "kductor"
                    text: i18n("No workspace selected")
                    explanation: i18n("Select a workspace from the sidebar or create a new one.")
                    helpfulAction: Kirigami.Action {
                        text: i18n("New Workspace")
                        icon.name: "list-add-symbolic"
                        onTriggered: createSheet.open()
                    }
                }

                // Loaded workspace
                Loader {
                    id: workspaceLoader
                    anchors.fill: parent
                    active: selectedWorkspaceId !== ""
                }
            }
        }
    }

    WorkspaceCreateSheet {
        id: createSheet
        onAccepted: WorkspaceModel.refresh()
    }

    FolderDialog {
        id: addRepoDialog
        title: i18n("Select Git Repository")
        onAccepted: {
            let path = selectedFolder.toString().replace("file://", "");
            if (GitManager.openRepository(path)) {
                WorkspaceModel.addRepo(path);
            }
        }
    }
}
