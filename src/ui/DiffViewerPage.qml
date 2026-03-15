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

    function reload() {
        diffData = GitManager.getDetailedDiff(worktreePath, sourceBranch);
        selectedFileIndex = 0;
    }

    // Empty state
    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        visible: diffData.length === 0
        width: parent.width - Kirigami.Units.largeSpacing * 4
        icon.name: "vcs-diff"
        text: i18n("No changes")
        explanation: i18n("The worktree has no modifications relative to the source branch.")
    }

    // Main content
    ColumnLayout {
        anchors.fill: parent
        visible: diffData.length > 0
        spacing: 0

        // File selector bar
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: i18n("%1 files changed", diffData.length)
                opacity: 0.6
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }

            QQC2.ComboBox {
                id: fileCombo
                Layout.fillWidth: true
                model: {
                    let items = [];
                    for (let i = 0; i < diffData.length; i++) {
                        let f = diffData[i];
                        let icon = f.status === 1 ? "+" : f.status === 2 ? "−" : f.status === 3 ? "~" : "?";
                        items.push(icon + "  " + f.newPath);
                    }
                    return items;
                }
                currentIndex: selectedFileIndex
                onCurrentIndexChanged: selectedFileIndex = currentIndex
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
                    if (s === 1) return Kirigami.Theme.positiveTextColor;  // Added
                    if (s === 2) return Kirigami.Theme.negativeTextColor;  // Deleted
                    return Kirigami.Theme.neutralTextColor;                // Modified etc
                }
            }

            QQC2.Label {
                text: selectedFileIndex < diffData.length ? diffData[selectedFileIndex].statusLabel : ""
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.6
            }

            QQC2.Label {
                text: selectedFileIndex < diffData.length ? diffData[selectedFileIndex].newPath : ""
                font.family: "monospace"
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
                        // Hunk header
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
                            return Kirigami.Theme.highlightColor.alpha(0.06);
                        if (modelData.type === "add")
                            return Qt.rgba(0.0, 0.7, 0.0, 0.08);
                        if (modelData.type === "del")
                            return Qt.rgba(0.8, 0.0, 0.0, 0.08);
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
                            font.family: "monospace"
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
                            font.family: "monospace"
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.35
                            rightPadding: Kirigami.Units.smallSpacing
                        }

                        QQC2.Label {
                            visible: !modelData.isHunkHeader
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 2.5
                            Layout.alignment: Qt.AlignTop
                            horizontalAlignment: Text.AlignRight
                            text: modelData.newLine
                            font.family: "monospace"
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            opacity: 0.35
                            rightPadding: Kirigami.Units.smallSpacing
                        }

                        // Gutter marker
                        QQC2.Label {
                            visible: !modelData.isHunkHeader
                            Layout.preferredWidth: Kirigami.Units.gridUnit
                            Layout.alignment: Qt.AlignTop
                            horizontalAlignment: Text.AlignHCenter
                            text: modelData.type === "add" ? "+" : modelData.type === "del" ? "−" : " "
                            font.family: "monospace"
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            font.bold: modelData.type !== "ctx"
                            color: modelData.type === "add" ? Kirigami.Theme.positiveTextColor
                                 : modelData.type === "del" ? Kirigami.Theme.negativeTextColor
                                 : Kirigami.Theme.textColor
                        }

                        // Content
                        QQC2.Label {
                            visible: !modelData.isHunkHeader
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            text: modelData.content
                            font.family: "monospace"
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            wrapMode: Text.WrapAnywhere
                            textFormat: Text.PlainText
                            color: modelData.type === "add" ? Kirigami.Theme.positiveTextColor
                                 : modelData.type === "del" ? Kirigami.Theme.negativeTextColor
                                 : Kirigami.Theme.textColor
                            opacity: modelData.type === "ctx" ? 0.7 : 1.0
                        }
                    }
                }
            }
        }
    }
}
