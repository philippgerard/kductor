import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

RowLayout {
    id: commandBar
    spacing: Kirigami.Units.smallSpacing

    signal promptSubmitted(string prompt)

    QQC2.TextField {
        id: promptInput
        Layout.fillWidth: true
        placeholderText: i18n("Type a prompt...")

        Keys.onReturnPressed: submitPrompt()
        Keys.onEnterPressed: submitPrompt()
    }

    QQC2.ToolButton {
        icon.name: "document-send-symbolic"
        enabled: promptInput.text.length > 0
        onClicked: submitPrompt()
        QQC2.ToolTip.text: i18n("Send")
        QQC2.ToolTip.visible: hovered
    }

    function submitPrompt() {
        if (promptInput.text.length === 0)
            return;
        commandBar.promptSubmitted(promptInput.text);
        promptInput.text = "";
    }
}
