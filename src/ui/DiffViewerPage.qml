import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kductor

Item {
    id: diffPage

    property string worktreePath: ""
    property string sourceBranch: ""
    property string workspaceName: ""

    property var diffData: []
    property int selectedFileIndex: 0
    property int diffMode: 0  // 0=all, 1=committed, 2=pending
    property bool loading: false
    property bool loadError: false

    function tint(c, a) { return Qt.rgba(c.r, c.g, c.b, a) }

    function reload() {
        // Remember the file currently under review so an agent-triggered reload
        // doesn't snap the user back to the first file.
        let keepPath = (selectedFileIndex < diffData.length) ? diffData[selectedFileIndex].newPath : "";
        diffPage._keepPath = keepPath;
        loading = true;
        loadError = false;
        GitManager.requestDetailedDiff(worktreePath, sourceBranch, diffMode);
    }
    property string _keepPath: ""

    Connections {
        target: GitManager
        function onDetailedDiffReady(files, mode, ok) {
            if (mode !== diffPage.diffMode) return; // a newer request superseded this
            diffPage.loading = false;
            diffPage.loadError = !ok;
            diffPage.diffData = files;
            // Restore the previously-selected file if it still exists.
            let idx = 0;
            if (diffPage._keepPath.length > 0) {
                for (let i = 0; i < files.length; i++) {
                    if (files[i].newPath === diffPage._keepPath) { idx = i; break; }
                }
            }
            diffPage.selectedFileIndex = idx;
        }
    }

    function currentFileDiffText() {
        if (selectedFileIndex >= diffData.length) return "";
        let file = diffData[selectedFileIndex];
        let out = file.newPath + "\n";
        let hunks = file.hunks || [];
        for (let h = 0; h < hunks.length; h++) {
            out += hunks[h].header + "\n";
            let lines = hunks[h].lines || [];
            for (let l = 0; l < lines.length; l++) {
                let p = lines[l].type === "add" ? "+" : lines[l].type === "del" ? "-" : " ";
                out += p + lines[l].content + "\n";
            }
        }
        return out;
    }

    // Loading indicator
    QQC2.BusyIndicator {
        anchors.centerIn: parent
        running: diffPage.loading
        visible: diffPage.loading
    }

    // Error state
    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: Kirigami.Units.gridUnit * 2
        visible: !diffPage.loading && diffPage.loadError
        width: parent.width - Kirigami.Units.largeSpacing * 4
        icon.name: "dialog-error-symbolic"
        text: i18n("Could not load the diff")
        explanation: i18n("The worktree could not be read. It may have been moved or removed.")
    }

    // Empty state (no changes)
    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: Kirigami.Units.gridUnit * 2
        visible: !diffPage.loading && !diffPage.loadError && diffData.length === 0
        width: parent.width - Kirigami.Units.largeSpacing * 4
        icon.name: "vcs-diff"
        text: i18n("No changes")
        explanation: diffMode === 1 ? i18n("No committed changes relative to the source branch.")
                   : diffMode === 2 ? i18n("No uncommitted changes in the worktree.")
                   : i18n("No modifications relative to the source branch.")
    }

    // Main content
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Mode and file selector bar
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            QQC2.TabBar {
                id: modeBar
                Layout.alignment: Qt.AlignVCenter
                currentIndex: diffMode
                onCurrentIndexChanged: {
                    diffMode = currentIndex;
                    reload();
                }
                QQC2.TabButton { text: i18n("All") }
                QQC2.TabButton { text: i18n("Committed") }
                QQC2.TabButton { text: i18n("Pending") }
            }

            QQC2.Label {
                text: i18np("%1 file", "%1 files", diffData.length)
                opacity: 0.7
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }

            QQC2.ComboBox {
                id: fileCombo
                Layout.fillWidth: true
                Accessible.name: i18n("Changed file")
                model: {
                    let items = [];
                    for (let i = 0; i < diffData.length; i++) {
                        let f = diffData[i];
                        let icon = (f.status === 1 || f.status === 7) ? "+" : f.status === 2 ? "−" : f.status === 3 ? "~" : "?";
                        items.push(icon + "  " + f.newPath);
                    }
                    return items;
                }
                currentIndex: selectedFileIndex
                // Only react to user selection; a model reset bouncing
                // currentIndex to 0 must not write back over selectedFileIndex.
                onActivated: selectedFileIndex = currentIndex
            }

            CopyButton {
                textToCopy: diffPage.currentFileDiffText()
                tooltip: i18n("Copy this file's diff")
                visible: diffData.length > 0
            }

            QQC2.ToolButton {
                icon.name: "view-refresh-symbolic"
                Accessible.name: i18n("Refresh diff")
                QQC2.ToolTip.text: i18n("Refresh")
                QQC2.ToolTip.visible: hovered
                onClicked: reload()
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        // File status header
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
            visible: diffData.length > 0

            Rectangle {
                width: Kirigami.Units.smallSpacing * 2
                height: Kirigami.Units.smallSpacing * 2
                radius: width / 2
                color: {
                    if (selectedFileIndex >= diffData.length) return "transparent";
                    let s = diffData[selectedFileIndex].status;
                    if (s === 1 || s === 7) return Kirigami.Theme.positiveTextColor;  // Added/untracked
                    if (s === 2) return Kirigami.Theme.negativeTextColor;  // Deleted
                    return Kirigami.Theme.neutralTextColor;                // Modified etc
                }
            }

            QQC2.Label {
                text: selectedFileIndex < diffData.length ? diffData[selectedFileIndex].statusLabel : ""
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
            }

            QQC2.Label {
                text: selectedFileIndex < diffData.length ? diffData[selectedFileIndex].newPath : ""
                font.family: Kirigami.Theme.fixedWidthFont.family
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        // Diff content
        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

            ListView {
                id: diffList
                clip: true
                model: buildLineModel()
                spacing: 0

                function buildLineModel() {
                    if (selectedFileIndex >= diffData.length) return [];
                    let file = diffData[selectedFileIndex];
                    let hunks = file.hunks || [];
                    let allLines = [];
                    for (let h = 0; h < hunks.length; h++) {
                        allLines.push({
                            isHunkHeader: true,
                            content: hunks[h].header,
                            type: "hdr",
                            oldLine: "",
                            newLine: ""
                        });
                        let lines = hunks[h].lines || [];
                        for (let l = 0; l < lines.length; l++) {
                            allLines.push({
                                isHunkHeader: false,
                                content: lines[l].content,
                                type: lines[l].type,
                                oldLine: lines[l].oldLine > 0 ? lines[l].oldLine.toString() : "",
                                newLine: lines[l].newLine > 0 ? lines[l].newLine.toString() : ""
                            });
                        }
                    }
                    return allLines;
                }

                delegate: Rectangle {
                    id: lineDel

                    required property var modelData
                    required property int index

                    width: diffList.width
                    height: lineRow.implicitHeight
                    color: {
                        if (modelData.isHunkHeader)
                            return diffPage.tint(Kirigami.Theme.highlightColor, 0.08);
                        if (modelData.type === "add")
                            return diffPage.tint(Kirigami.Theme.positiveTextColor, 0.12);
                        if (modelData.type === "del")
                            return diffPage.tint(Kirigami.Theme.negativeTextColor, 0.12);
                        return "transparent";
                    }

                    RowLayout {
                        id: lineRow
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 0

                        // Hunk header (full width)
                        QQC2.Label {
                            visible: modelData.isHunkHeader
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.smallSpacing
                            Layout.topMargin: Kirigami.Units.smallSpacing
                            Layout.bottomMargin: Kirigami.Units.smallSpacing
                            text: modelData.content
                            font.family: Kirigami.Theme.fixedWidthFont.family
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            color: Kirigami.Theme.disabledTextColor
                        }

                        // Line numbers + content (normal lines)
                        QQC2.Label {
                            visible: !modelData.isHunkHeader
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 2.5
                            Layout.alignment: Qt.AlignTop
                            horizontalAlignment: Text.AlignRight
                            text: modelData.oldLine
                            font.family: Kirigami.Theme.fixedWidthFont.family
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.5
                            rightPadding: Kirigami.Units.smallSpacing
                        }

                        QQC2.Label {
                            visible: !modelData.isHunkHeader
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 2.5
                            Layout.alignment: Qt.AlignTop
                            horizontalAlignment: Text.AlignRight
                            text: modelData.newLine
                            font.family: Kirigami.Theme.fixedWidthFont.family
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.5
                            rightPadding: Kirigami.Units.smallSpacing
                        }

                        // Gutter marker
                        QQC2.Label {
                            visible: !modelData.isHunkHeader
                            Layout.preferredWidth: Kirigami.Units.gridUnit
                            Layout.alignment: Qt.AlignTop
                            horizontalAlignment: Text.AlignHCenter
                            text: modelData.type === "add" ? "+" : modelData.type === "del" ? "−" : " "
                            font.family: Kirigami.Theme.fixedWidthFont.family
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            font.bold: modelData.type !== "ctx"
                            color: modelData.type === "add" ? Kirigami.Theme.positiveTextColor
                                 : modelData.type === "del" ? Kirigami.Theme.negativeTextColor
                                 : Kirigami.Theme.textColor
                        }

                        // Content (selectable)
                        TextEdit {
                            visible: !modelData.isHunkHeader
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            text: modelData.content
                            font.family: Kirigami.Theme.fixedWidthFont.family
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            wrapMode: Text.WrapAnywhere
                            textFormat: Text.PlainText
                            readOnly: true
                            selectByMouse: true
                            selectionColor: Kirigami.Theme.highlightColor
                            color: modelData.type === "add" ? Kirigami.Theme.positiveTextColor
                                 : modelData.type === "del" ? Kirigami.Theme.negativeTextColor
                                 : Kirigami.Theme.textColor
                            opacity: modelData.type === "ctx" ? 0.8 : 1.0
                        }
                    }
                }
            }
        }
    }
}
