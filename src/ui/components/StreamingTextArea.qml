import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.ScrollView {
    id: streamView

    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

    property alias model: listView.model

    ListView {
        id: listView
        clip: true
        spacing: 0
        leftMargin: Kirigami.Units.largeSpacing
        rightMargin: Kirigami.Units.largeSpacing
        topMargin: Kirigami.Units.smallSpacing
        bottomMargin: Kirigami.Units.smallSpacing

        onCountChanged: {
            Qt.callLater(function() {
                listView.positionViewAtEnd();
            });
        }

        // Track which tool-use group indices are expanded
        property var expandedToolGroups: ({})

        delegate: Item {
            id: lineDelegate

            required property string content
            required property int lineType
            required property string toolName
            required property int index

            // For tool use lines: is this the first in a consecutive run?
            readonly property bool isToolUse: lineType === 2
            readonly property bool prevIsToolUse: index > 0 && listView.model.data(listView.model.index(index - 1, 0), 258) === 2 // LineTypeRole = 258
            readonly property bool isGroupStart: isToolUse && !prevIsToolUse
            readonly property bool isGroupMember: isToolUse && prevIsToolUse

            // Find which group start index this tool use belongs to
            function findGroupStart() {
                let i = index;
                while (i > 0) {
                    let prevType = listView.model.data(listView.model.index(i - 1, 0), 258);
                    if (prevType !== 2) break;
                    i--;
                }
                return i;
            }

            // Count consecutive tool uses from a start index
            function countGroupSize(startIdx) {
                let count = 0;
                let total = listView.model.rowCount();
                for (let i = startIdx; i < total; i++) {
                    if (listView.model.data(listView.model.index(i, 0), 258) !== 2) break;
                    count++;
                }
                return count;
            }

            readonly property int groupStartIdx: isToolUse ? findGroupStart() : -1
            readonly property bool groupExpanded: groupStartIdx >= 0 && listView.expandedToolGroups[groupStartIdx] === true

            width: listView.width - listView.leftMargin - listView.rightMargin
            height: lineContent.implicitHeight
            // lineType: 0=Text, 1=Thinking, 2=ToolUse, 3=ToolResult, 4=System, 5=Error

            ColumnLayout {
                id: lineContent
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 0

                // Agent text — the primary content, with markdown
                QQC2.Label {
                    visible: lineType === 0
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    text: content
                    wrapMode: Text.Wrap
                    textFormat: Text.MarkdownText
                }

                // Thinking — collapsible
                ColumnLayout {
                    visible: lineType === 1
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    Layout.bottomMargin: 2
                    spacing: 0

                    QQC2.AbstractButton {
                        Layout.fillWidth: true
                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Kirigami.Icon {
                                source: thinkingContent.visible ? "arrow-down-symbolic" : "arrow-right-symbolic"
                                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                opacity: 0.4
                            }
                            QQC2.Label {
                                text: i18n("Thinking…")
                                font.italic: true
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                opacity: 0.4
                            }
                            Item { Layout.fillWidth: true }
                        }
                        onClicked: thinkingContent.visible = !thinkingContent.visible
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }

                    QQC2.Label {
                        id: thinkingContent
                        visible: false
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing
                        text: content
                        wrapMode: Text.Wrap
                        font.italic: true
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.5
                        textFormat: Text.PlainText
                    }
                }

                // Tool use — collapsible group header (only on first of consecutive run)
                ColumnLayout {
                    visible: isGroupStart
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    Layout.bottomMargin: 2
                    spacing: 0

                    QQC2.AbstractButton {
                        Layout.fillWidth: true
                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Kirigami.Icon {
                                source: groupExpanded ? "arrow-down-symbolic" : "arrow-right-symbolic"
                                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                opacity: 0.4
                            }
                            QQC2.Label {
                                text: {
                                    let n = countGroupSize(lineDelegate.index);
                                    return i18np("%1 action", "%1 actions", n);
                                }
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                opacity: 0.4
                            }
                            Item { Layout.fillWidth: true }
                        }
                        onClicked: {
                            let groups = listView.expandedToolGroups;
                            groups[lineDelegate.index] = !groups[lineDelegate.index];
                            listView.expandedToolGroups = groups;
                        }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }
                }

                // Tool use — individual line (visible only when group is expanded)
                RowLayout {
                    visible: isToolUse && groupExpanded
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing

                    Rectangle {
                        width: 2
                        Layout.fillHeight: true
                        color: Kirigami.Theme.focusColor
                        opacity: 0.3
                        radius: 1
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: content
                        font.family: "monospace"
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: Kirigami.Theme.focusColor
                        opacity: 0.5
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }
                }

                // Tool use — group member hidden (takes no space when collapsed)
                Item {
                    visible: isGroupMember && !groupExpanded
                    height: 0
                }

                // Tool result — hidden by default (too verbose)
                Item {
                    visible: lineType === 3
                    height: 0
                }

                // User message — prominent but distinct from agent replies
                RowLayout {
                    visible: lineType === 4
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.largeSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    spacing: Kirigami.Units.smallSpacing

                    Rectangle {
                        width: 3
                        Layout.fillHeight: true
                        radius: 1
                        color: Kirigami.Theme.highlightColor
                        opacity: 0.5
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: content
                        wrapMode: Text.Wrap
                        font.bold: true
                        textFormat: Text.PlainText
                    }
                }

                // Error
                QQC2.Label {
                    visible: lineType === 5
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    Layout.bottomMargin: 2
                    text: content
                    wrapMode: Text.Wrap
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.negativeTextColor
                    textFormat: Text.PlainText
                }
            }
        }
    }
}
