import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kductor

Kirigami.Dialog {
    id: createDialog

    title: selectedRepo ? i18n("New Workspace — %1", selectedRepo.split("/").pop()) : i18n("New Workspace")
    preferredWidth: Kirigami.Units.gridUnit * 22

    signal accepted()

    property string selectedRepo: ""
    property var branches: []
    property string errorMessage: ""

    readonly property var cityNames: [
        "lisbon", "kyoto", "oslo", "quito", "nairobi", "vienna", "porto", "havana",
        "reykjavik", "tallinn", "marrakesh", "osaka", "bergen", "valencia", "bologna",
        "kigali", "montreal", "helsinki", "dublin", "krakow", "geneva", "antwerp",
        "cusco", "sapporo", "brisbane", "wellington", "zagreb", "ljubljana", "riga",
        "vilnius", "oaxaca", "mendoza", "bordeaux", "seville", "palermo", "turin",
        "ghent", "lucerne", "salzburg", "granada", "kanazawa", "busan", "taipei", "hanoi"
    ]

    function openForRepo(repoPath) {
        selectedRepo = repoPath;
        open();
    }

    function createWorkspace() {
        if (nameField.text.length === 0 || selectedRepo.length === 0) {
            return;
        }
        let branch = branchCombo.currentText || "main";
        let success = WorktreeManager.createWorkspace(nameField.text, selectedRepo, branch);
        if (success) {
            createDialog.accepted();
            createDialog.close();
        }
    }

    onOpened: {
        nameField.text = cityNames[Math.floor(Math.random() * cityNames.length)];
        errorMessage = "";
        branches = [];
        if (selectedRepo.length > 0 && GitManager.openRepository(selectedRepo)) {
            branches = GitManager.listBranches();
            branchCombo.model = branches;
            if (branches.length > 0) {
                let mainIdx = branches.indexOf("main");
                if (mainIdx < 0) mainIdx = branches.indexOf("master");
                if (mainIdx >= 0) branchCombo.currentIndex = mainIdx;
            }
        }
        nameField.forceActiveFocus();
        nameField.selectAll();
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
                Accessible.name: i18n("Workspace name")
                onAccepted: createDialog.createWorkspace()
            }

            QQC2.ComboBox {
                id: branchCombo
                Kirigami.FormData.label: i18n("Source branch:")
                Layout.fillWidth: true
                model: branches
                enabled: branches.length > 0
                Accessible.name: i18n("Source branch")
            }
        }
    }

    customFooterActions: [
        Kirigami.Action {
            text: i18n("Create")
            icon.name: "list-add-symbolic"
            enabled: nameField.text.length > 0 && selectedRepo.length > 0
            onTriggered: createDialog.createWorkspace()
        }
    ]
}
