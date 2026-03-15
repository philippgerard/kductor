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

        delegate: Item {
            id: lineDelegate

            required property string content
            required property int lineType
            required property string toolName
            required property int index

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

                // Tool use — compact mono line
                RowLayout {
                    visible: lineType === 2
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    Layout.bottomMargin: 2
                    spacing: Kirigami.Units.smallSpacing

                    Rectangle {
                        width: 3
                        Layout.fillHeight: true
                        color: Kirigami.Theme.focusColor
                        opacity: 0.6
                        radius: 1
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: content
                        font.family: "monospace"
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: Kirigami.Theme.focusColor
                        opacity: 0.7
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }
                }

                // Tool result — hidden by default (too verbose)
                Item {
                    visible: lineType === 3
                    height: 0
                }

                // System — small muted line
                QQC2.Label {
                    visible: lineType === 4
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    text: content
                    wrapMode: Text.Wrap
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.45
                    textFormat: Text.PlainText
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
