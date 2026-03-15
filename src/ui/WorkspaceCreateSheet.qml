import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kductor

Kirigami.Dialog {
    id: createDialog

    title: i18n("New Workspace")
    preferredWidth: Kirigami.Units.gridUnit * 25

    signal accepted()

    property string selectedRepo: ""
    property string prefillRepo: ""
    property var branches: []
    property string errorMessage: ""

    function openForRepo(repoPath) {
        prefillRepo = repoPath;
        open();
    }

    onOpened: {
        nameField.text = "";
        errorMessage = "";
        if (prefillRepo.length > 0) {
            selectedRepo = prefillRepo;
            repoPathField.text = prefillRepo;
            prefillRepo = "";
            // Load branches for pre-filled repo
            if (GitManager.openRepository(selectedRepo)) {
                branches = GitManager.listBranches();
                branchCombo.model = branches;
                if (branches.length > 0) {
                    let mainIdx = branches.indexOf("main");
                    if (mainIdx < 0) mainIdx = branches.indexOf("master");
                    if (mainIdx >= 0) branchCombo.currentIndex = mainIdx;
                }
            }
        } else {
            selectedRepo = "";
            repoPathField.text = "";
            branchCombo.model = [];
        }
        nameField.forceActiveFocus();
    }

    Connections {
        target: WorktreeManager
        function onErrorOccurred(message) {
            errorMessage = message;
        }
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            text: errorMessage
            visible: errorMessage.length > 0
        }

        Kirigami.FormLayout {
            QQC2.TextField {
                id: nameField
                Kirigami.FormData.label: i18n("Workspace name:")
                placeholderText: i18n("e.g., fix-login-bug")
            }

            RowLayout {
                Kirigami.FormData.label: i18n("Repository:")
                QQC2.TextField {
                    id: repoPathField
                    Layout.fillWidth: true
                    placeholderText: i18n("/path/to/your/repo")
                    readOnly: true
                    text: selectedRepo
                }
                QQC2.Button {
                    icon.name: "folder-open-symbolic"
                    text: i18n("Browse")
                    onClicked: folderDialog.open()
                }
            }

            QQC2.ComboBox {
                id: branchCombo
                Kirigami.FormData.label: i18n("Source branch:")
                Layout.fillWidth: true
                model: branches
                enabled: branches.length > 0
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: i18n("Select Git Repository")
        onAccepted: {
            let path = selectedFolder.toString().replace("file://", "");
            selectedRepo = path;
            repoPathField.text = path;

            if (GitManager.openRepository(path)) {
                branches = GitManager.listBranches();
                branchCombo.model = branches;
                if (branches.length > 0) {
                    let mainIdx = branches.indexOf("main");
                    if (mainIdx < 0) mainIdx = branches.indexOf("master");
                    if (mainIdx >= 0) branchCombo.currentIndex = mainIdx;
                }
            }
        }
    }

    customFooterActions: [
        Kirigami.Action {
            text: i18n("Create")
            icon.name: "list-add-symbolic"
            enabled: nameField.text.length > 0 && selectedRepo.length > 0
            onTriggered: {
                let branch = branchCombo.currentText || "main";
                let success = WorktreeManager.createWorkspace(nameField.text, selectedRepo, branch);
                if (success) {
                    createDialog.accepted();
                    createDialog.close();
                }
            }
        }
    ]
}
