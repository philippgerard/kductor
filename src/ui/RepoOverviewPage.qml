import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kductor

Item {
    id: repoPage

    required property string repoPath
    readonly property string repoName: repoPath.split("/").pop()
    property var branches: []
    property var workspaces: []

    Component.onCompleted: reload()

    function reload() {
        if (GitManager.openRepository(repoPath)) {
            branches = GitManager.listBranches();
        }
        workspaces = WorkspaceModel.count >= 0 ? WorkspaceModel.workspacesForRepo(repoPath) : [];
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing * 2
        spacing: Kirigami.Units.largeSpacing

        // Repo header
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Icon {
                source: "folder-git-symbolic"
                Layout.preferredWidth: Kirigami.Units.iconSizes.large
                Layout.preferredHeight: Kirigami.Units.iconSizes.large
            }

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true

                Kirigami.Heading {
                    text: repoName
                    level: 2
                }
                QQC2.Label {
                    text: repoPath
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.6
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        // Stats
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing * 2

            ColumnLayout {
                spacing: 2
                QQC2.Label {
                    text: branches.length.toString()
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.5
                    font.bold: true
                }
                QQC2.Label {
                    text: i18n("Branches")
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.6
                }
            }

            ColumnLayout {
                spacing: 2
                QQC2.Label {
                    text: workspaces.length.toString()
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.5
                    font.bold: true
                }
                QQC2.Label {
                    text: i18n("Workspaces")
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.6
                }
            }

            ColumnLayout {
                spacing: 2
                QQC2.Label {
                    text: {
                        let count = 0;
                        for (let ws of workspaces) {
                            count += AgentManager.agentsForWorkspace(ws.id).length;
                        }
                        return count.toString();
                    }
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.5
                    font.bold: true
                }
                QQC2.Label {
                    text: i18n("Agents")
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.6
                }
            }

            Item { Layout.fillWidth: true }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        // Workspaces list
        QQC2.Label {
            text: i18n("Workspaces")
            font.bold: true
            visible: workspaces.length > 0
        }

        Repeater {
            model: workspaces
            delegate: QQC2.ItemDelegate {
                Layout.fillWidth: true
                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.Icon {
                        source: "vcs-branch"
                        Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                        Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        opacity: 0.6
                    }
                    QQC2.Label {
                        text: modelData.name
                        Layout.fillWidth: true
                    }
                    QQC2.Label {
                        text: modelData.branchName
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.5
                        elide: Text.ElideRight
                    }
                }
                onClicked: {
                    applicationWindow().selectedWorkspaceId = modelData.id;
                    applicationWindow().selectedWorkspaceName = modelData.name;
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

        // New workspace button
        QQC2.Button {
            icon.name: "list-add-symbolic"
            text: i18n("New Workspace")
            onClicked: createSheet.openForRepo(repoPath)
        }

        Item { Layout.fillHeight: true }
    }
}
