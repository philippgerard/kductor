import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: commandBar
    spacing: Kirigami.Units.smallSpacing

    signal promptSubmitted(string prompt, var imagePaths)

    property var attachedImages: []
    property bool dropHighlight: false

    function focusInput() { promptInput.forceActiveFocus() }
    function addImage(path) {
        let imgs = attachedImages.slice();
        if (imgs.indexOf(path) === -1) {
            imgs.push(path);
            attachedImages = imgs;
        }
    }
    function removeImage(index) {
        let imgs = attachedImages.slice();
        imgs.splice(index, 1);
        attachedImages = imgs;
    }
    function clearImages() { attachedImages = [] }

    Component.onCompleted: Qt.callLater(focusInput)

    // Image attachment previews
    Flow {
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing
        visible: attachedImages.length > 0

        Repeater {
            model: commandBar.attachedImages

            Rectangle {
                width: Kirigami.Units.gridUnit * 5
                height: Kirigami.Units.gridUnit * 5
                radius: Kirigami.Units.cornerRadius
                color: Kirigami.Theme.alternateBackgroundColor
                border.color: Kirigami.Theme.disabledTextColor
                border.width: 1

                Image {
                    anchors.fill: parent
                    anchors.margins: 2
                    source: "file://" + modelData
                    fillMode: Image.PreserveAspectFit
                    sourceSize.width: width * 2
                    sourceSize.height: height * 2
                }

                QQC2.ToolButton {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: -Kirigami.Units.smallSpacing
                    width: Kirigami.Units.gridUnit * 1.5
                    height: width
                    icon.name: "edit-delete-remove-symbolic"
                    icon.width: Kirigami.Units.iconSizes.small
                    icon.height: Kirigami.Units.iconSizes.small
                    onClicked: commandBar.removeImage(index)
                    QQC2.ToolTip.text: i18n("Remove")
                    QQC2.ToolTip.visible: hovered
                }
            }
        }
    }

    RowLayout {
        spacing: Kirigami.Units.smallSpacing

        QQC2.TextField {
            id: promptInput
            Layout.fillWidth: true
            placeholderText: commandBar.attachedImages.length > 0
                ? i18n("Describe the screenshot (optional)...")
                : i18n("Type a prompt...")
            focus: true

            Keys.onReturnPressed: submitPrompt()
            Keys.onEnterPressed: submitPrompt()
        }

        QQC2.ToolButton {
            icon.name: "document-send-symbolic"
            enabled: promptInput.text.length > 0 || commandBar.attachedImages.length > 0
            onClicked: submitPrompt()
            QQC2.ToolTip.text: i18n("Send")
            QQC2.ToolTip.visible: hovered
        }
    }

    function submitPrompt() {
        if (promptInput.text.length === 0 && attachedImages.length === 0)
            return;
        commandBar.promptSubmitted(promptInput.text, attachedImages.slice());
        promptInput.text = "";
        clearImages();
        promptInput.forceActiveFocus();
    }
}
