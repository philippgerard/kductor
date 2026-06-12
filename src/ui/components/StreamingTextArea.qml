import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.ScrollView {
    id: streamView

    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

    property alias model: listView.model

    // LineTypeRole = Qt.UserRole + 2 (see AgentOutputModel::Roles)
    readonly property int lineTypeRole: 258

    ListView {
        id: listView
        clip: true
        spacing: 0
        leftMargin: Kirigami.Units.largeSpacing
        rightMargin: Kirigami.Units.largeSpacing
        topMargin: Kirigami.Units.smallSpacing
        bottomMargin: Kirigami.Units.smallSpacing
        activeFocusOnTab: true
        Accessible.role: Accessible.List
        Accessible.name: i18n("Agent output")

        // Only follow the stream when the user is already at the bottom, so
        // scrolling back to read older output is not yanked away on each line.
        property bool autoScroll: true
        onMovementEnded: autoScroll = atYEnd
        onCountChanged: {
            if (autoScroll)
                Qt.callLater(function() { listView.positionViewAtEnd(); });
        }

        // Keyboard scrolling for accessibility.
        Keys.onPressed: function(event) {
            const page = listView.height * 0.9;
            if (event.key === Qt.Key_PageDown) {
                listView.contentY = Math.min(listView.contentY + page,
                    listView.contentHeight - listView.height + listView.bottomMargin);
                event.accepted = true;
            } else if (event.key === Qt.Key_PageUp) {
                listView.contentY = Math.max(listView.contentY - page, -listView.topMargin);
                event.accepted = true;
            } else if (event.key === Qt.Key_End) {
                listView.positionViewAtEnd();
                event.accepted = true;
            } else if (event.key === Qt.Key_Home) {
                listView.positionViewAtBeginning();
                event.accepted = true;
            }
        }

        // Track which tool-activity group (by start index) is expanded.
        property var expandedToolGroups: ({})

        function typeAt(i) {
            if (i < 0 || i >= listView.model.rowCount()) return -1;
            return listView.model.data(listView.model.index(i, 0), streamView.lineTypeRole);
        }
        // Tool activity = a tool_use (2) or its tool_result (3).
        function isToolActivity(t) { return t === 2 || t === 3; }

        delegate: Item {
            id: lineDelegate

            required property string content
            required property int lineType
            required property string toolName
            required property int index

            readonly property bool isTool: listView.isToolActivity(lineType)
            readonly property bool prevIsTool: index > 0 && listView.isToolActivity(listView.typeAt(index - 1))
            readonly property bool isGroupStart: isTool && !prevIsTool

            function findGroupStart() {
                let i = index;
                while (i > 0 && listView.isToolActivity(listView.typeAt(i - 1)))
                    i--;
                return i;
            }
            // Number of tool *uses* (actions) in this consecutive run.
            function countActions(startIdx) {
                let count = 0;
                let total = listView.model.rowCount();
                for (let i = startIdx; i < total; i++) {
                    let t = listView.typeAt(i);
                    if (!listView.isToolActivity(t)) break;
                    if (t === 2) count++;
                }
                return count;
            }

            readonly property int groupStartIdx: isTool ? findGroupStart() : -1
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
                TextEdit {
                    visible: lineType === 0
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    text: content
                    wrapMode: Text.Wrap
                    textFormat: Text.MarkdownText
                    readOnly: true
                    selectByMouse: true
                    color: Kirigami.Theme.textColor
                    selectedTextColor: Kirigami.Theme.highlightedTextColor
                    selectionColor: Kirigami.Theme.highlightColor
                    font: Kirigami.Theme.defaultFont
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
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: i18n("Thinking, %1", thinkingContent.visible ? i18n("expanded") : i18n("collapsed"))
                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Kirigami.Icon {
                                source: thinkingContent.visible ? "arrow-down-symbolic" : "arrow-right-symbolic"
                                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                opacity: 0.6
                            }
                            QQC2.Label {
                                text: i18n("Thinking…")
                                font.italic: true
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                opacity: 0.6
                            }
                            Item { Layout.fillWidth: true }
                        }
                        onClicked: thinkingContent.visible = !thinkingContent.visible
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }

                    TextEdit {
                        id: thinkingContent
                        visible: false
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing
                        text: content
                        wrapMode: Text.Wrap
                        font.italic: true
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                        textFormat: Text.PlainText
                        readOnly: true
                        selectByMouse: true
                        color: Kirigami.Theme.textColor
                        selectionColor: Kirigami.Theme.highlightColor
                    }
                }

                // Tool activity — collapsible group header (only on first of a run)
                ColumnLayout {
                    visible: isGroupStart
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    Layout.bottomMargin: 2
                    spacing: 0

                    QQC2.AbstractButton {
                        Layout.fillWidth: true
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: {
                            let n = countActions(lineDelegate.index);
                            return i18np("%1 action, %2", "%1 actions, %2", n,
                                         groupExpanded ? i18n("expanded") : i18n("collapsed"));
                        }
                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Kirigami.Icon {
                                source: groupExpanded ? "arrow-down-symbolic" : "arrow-right-symbolic"
                                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                opacity: 0.6
                            }
                            QQC2.Label {
                                text: {
                                    let n = countActions(lineDelegate.index);
                                    return i18np("%1 action", "%1 actions", n);
                                }
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                opacity: 0.6
                            }
                            Item { Layout.fillWidth: true }
                        }
                        onClicked: {
                            let groups = Object.assign({}, listView.expandedToolGroups);
                            groups[lineDelegate.index] = !groups[lineDelegate.index];
                            listView.expandedToolGroups = groups;
                        }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }
                }

                // Tool use — individual action line (visible when group expanded)
                RowLayout {
                    visible: lineType === 2 && groupExpanded
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing
                    Layout.topMargin: 1
                    spacing: Kirigami.Units.smallSpacing

                    Rectangle {
                        width: 2
                        Layout.fillHeight: true
                        color: Kirigami.Theme.focusColor
                        opacity: 0.4
                        radius: 1
                    }
                    TextEdit {
                        Layout.fillWidth: true
                        text: content
                        font.family: Kirigami.Theme.fixedWidthFont.family
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: Kirigami.Theme.focusColor
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        readOnly: true
                        selectByMouse: true
                        selectionColor: Kirigami.Theme.highlightColor
                    }
                }

                // Tool result — shown indented under the actions when expanded
                ColumnLayout {
                    visible: lineType === 3 && groupExpanded
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.iconSizes.small + Kirigami.Units.largeSpacing
                    spacing: 0

                    TextEdit {
                        Layout.fillWidth: true
                        text: content.length > 4000 ? content.substring(0, 4000) + "\n…" : content
                        font.family: Kirigami.Theme.fixedWidthFont.family
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        readOnly: true
                        selectByMouse: true
                        selectionColor: Kirigami.Theme.highlightColor
                    }
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

                    TextEdit {
                        Layout.fillWidth: true
                        text: content
                        wrapMode: Text.Wrap
                        font.bold: true
                        textFormat: Text.PlainText
                        readOnly: true
                        selectByMouse: true
                        color: Kirigami.Theme.textColor
                        selectedTextColor: Kirigami.Theme.highlightedTextColor
                        selectionColor: Kirigami.Theme.highlightColor
                        font.family: Kirigami.Theme.defaultFont.family
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize
                    }
                }

                // Error
                TextEdit {
                    visible: lineType === 5
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    Layout.bottomMargin: 2
                    text: content
                    wrapMode: Text.Wrap
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.negativeTextColor
                    textFormat: Text.PlainText
                    readOnly: true
                    selectByMouse: true
                    selectionColor: Kirigami.Theme.highlightColor
                }
            }
        }
    }

    // Jump-to-bottom affordance when the user has scrolled up during a stream.
    QQC2.ToolButton {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Kirigami.Units.largeSpacing
        visible: !listView.autoScroll
        icon.name: "go-bottom-symbolic"
        Accessible.name: i18n("Scroll to latest output")
        QQC2.ToolTip.text: i18n("Jump to latest")
        QQC2.ToolTip.visible: hovered
        onClicked: {
            listView.autoScroll = true;
            listView.positionViewAtEnd();
        }
    }
}
