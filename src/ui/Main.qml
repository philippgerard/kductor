import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
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

                    // Sidebar header
                    QQC2.ToolBar {
                        Layout.fillWidth: true
                        contentItem: RowLayout {
                            QQC2.Label {
                                text: i18n("Workspaces")
                                font.bold: true
                                Layout.fillWidth: true
                            }
                            QQC2.ToolButton {
                                icon.name: "list-add-symbolic"
                                onClicked: createSheet.open()
                                QQC2.ToolTip.text: i18n("New Workspace")
                                QQC2.ToolTip.visible: hovered
                            }
                        }
                    }

                    // Workspace list
                    QQC2.ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

                        ListView {
                            id: workspaceList
                            model: WorkspaceModel
                            clip: true

                            delegate: QQC2.ItemDelegate {
                                id: wsDelegate
                                width: workspaceList.width
                                highlighted: workspaceId === selectedWorkspaceId

                                required property int index
                                required property string workspaceId
                                required property string name
                                required property string branchName
                                required property int status

                                contentItem: ColumnLayout {
                                    spacing: 2

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing

                                        // Status dot
                                        Rectangle {
                                            width: Kirigami.Units.smallSpacing * 2
                                            height: width
                                            radius: width / 2
                                            color: {
                                                let running = AgentManager.activeCount >= 0 && AgentManager.workspaceAgentStatus(wsDelegate.workspaceId) === 2;
                                                if (running) return Kirigami.Theme.focusColor;
                                                if (wsDelegate.status === 2) return Kirigami.Theme.disabledTextColor;
                                                return Kirigami.Theme.positiveTextColor;
                                            }

                                            SequentialAnimation on opacity {
                                                running: AgentManager.activeCount >= 0 && AgentManager.workspaceAgentStatus(wsDelegate.workspaceId) === 2
                                                loops: Animation.Infinite
                                                NumberAnimation { to: 0.3; duration: 800; easing.type: Easing.InOutQuad }
                                                NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
                                            }
                                        }

                                        QQC2.Label {
                                            text: wsDelegate.name
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                            font.bold: wsDelegate.workspaceId === selectedWorkspaceId
                                        }
                                    }

                                    QQC2.Label {
                                        text: wsDelegate.branchName
                                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                                        opacity: 0.5
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                        Layout.leftMargin: Kirigami.Units.smallSpacing * 2 + Kirigami.Units.smallSpacing
                                    }
                                }

                                onClicked: {
                                    let wsData = WorkspaceModel.get(wsDelegate.index);
                                    selectedWorkspaceId = wsData.id;
                                    selectedWorkspaceName = wsData.name;
                                    workspaceLoader.setSource(Qt.resolvedUrl("WorkspacePage.qml"), {
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
}
