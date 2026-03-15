import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

RowLayout {
    id: commandBar
    spacing: Kirigami.Units.smallSpacing

    signal promptSubmitted(string prompt)

    Layout.margins: Kirigami.Units.smallSpacing

    QQC2.TextField {
        id: promptInput
        Layout.fillWidth: true
        placeholderText: i18n("Send a prompt to the agent...")
        font.family: "monospace"

        Keys.onReturnPressed: function(event) {
            if (event.modifiers & Qt.ControlModifier) {
                submitPrompt();
            }
        }
        Keys.onEnterPressed: function(event) {
            if (event.modifiers & Qt.ControlModifier) {
                submitPrompt();
            }
        }
    }

    QQC2.Button {
        icon.name: "document-send-symbolic"
        text: i18n("Send")
        enabled: promptInput.text.length > 0
        onClicked: submitPrompt()

        QQC2.ToolTip.text: i18n("Send prompt (Ctrl+Enter)")
        QQC2.ToolTip.visible: hovered
    }

    function submitPrompt() {
        if (promptInput.text.length === 0)
            return;
        commandBar.promptSubmitted(promptInput.text);
        promptInput.text = "";
    }
}
