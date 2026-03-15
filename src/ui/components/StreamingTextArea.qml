import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.ScrollView {
    id: streamView

    property alias model: listView.model

    ListView {
        id: listView
        clip: true
        spacing: 2

        onCountChanged: {
            Qt.callLater(function() {
                listView.positionViewAtEnd();
            });
        }

        delegate: Rectangle {
            id: lineDelegate

            required property string content
            required property int lineType
            required property string toolName
            required property int index

            width: listView.width
            height: lineContent.implicitHeight + Kirigami.Units.smallSpacing * 2
            radius: 2

            color: {
                switch (lineType) {
                case 0: return "transparent";                                          // Text
                case 1: return Kirigami.Theme.backgroundColor;                         // Thinking
                case 2: return Kirigami.Theme.focusColor.alpha(0.08);                  // ToolUse
                case 3: return Kirigami.Theme.backgroundColor;                         // ToolResult
                case 4: return Kirigami.Theme.highlightColor.alpha(0.05);              // System
                case 5: return Kirigami.Theme.negativeBackgroundColor.alpha(0.1);      // Error
                default: return "transparent";
                }
            }

            ColumnLayout {
                id: lineContent
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                spacing: 0

                // Tool use header
                QQC2.Label {
                    visible: lineType === 2 && toolName.length > 0
                    text: toolName
                    font.bold: true
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.focusColor
                    opacity: 0.8
                }

                // System/error prefix
                QQC2.Label {
                    visible: lineType === 4 || lineType === 5
                    text: lineType === 5 ? i18n("Error") : i18n("System")
                    font.bold: true
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: lineType === 5 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.disabledTextColor
                }

                // Main content
                QQC2.Label {
                    Layout.fillWidth: true
                    text: content
                    wrapMode: Text.Wrap
                    font.family: lineType <= 1 ? Kirigami.Theme.defaultFont.family : "monospace"
                    font.italic: lineType === 1 // Thinking
                    opacity: lineType === 1 ? 0.6 : 1.0
                    color: lineType === 5 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                    textFormat: Text.PlainText
                }
            }
        }
    }
}
