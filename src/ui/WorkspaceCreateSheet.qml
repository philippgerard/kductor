import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kductor

Kirigami.Dialog {
    id: createDialog

    title: i18n("New Workspace")
    standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
    preferredWidth: Kirigami.Units.gridUnit * 25

    signal accepted()

    property string selectedRepo: ""
    property var branches: []

    onOpened: {
        nameField.text = "";
        repoPathField.text = "";
        selectedRepo = "";
        branchCombo.model = [];
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

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

    onRejected: close()

    Component.onCompleted: {
        // Connect the OK button
        let okButton = standardButton(Kirigami.Dialog.Ok);
    }

    function doAccept() {
        if (nameField.text.length === 0 || selectedRepo.length === 0) {
            return;
        }

        let branch = branchCombo.currentText || "main";
        let success = WorktreeManager.createWorkspace(nameField.text, selectedRepo, branch);

        if (success) {
            accepted();
            close();
        }
    }

    // Override the Ok button behavior
    onClosed: {}
    Component.onDestruction: {}

    footer: QQC2.DialogButtonBox {
        QQC2.Button {
            text: i18n("Cancel")
            QQC2.DialogButtonBox.buttonRole: QQC2.DialogButtonBox.RejectRole
            onClicked: createDialog.close()
        }
        QQC2.Button {
            text: i18n("Create")
            icon.name: "list-add-symbolic"
            QQC2.DialogButtonBox.buttonRole: QQC2.DialogButtonBox.AcceptRole
            enabled: nameField.text.length > 0 && selectedRepo.length > 0
            onClicked: createDialog.doAccept()
        }
    }
}
