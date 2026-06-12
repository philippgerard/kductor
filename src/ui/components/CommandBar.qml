import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: commandBar
    spacing: Kirigami.Units.smallSpacing

    signal promptSubmitted(string prompt, var imagePaths)
    signal attachRequested()

    property var attachedImages: []
    // Set by the host while the agent is working; disables sending.
    property bool busy: false

    // Prompt history for arrow-up recall.
    property var history: []
    property int historyIndex: -1
    property string draft: ""

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

    // Called by the host once a prompt was actually accepted, so rejected
    // prompts keep their text instead of vanishing.
    function acceptSubmit() {
        let text = promptInput.text.trim();
        if (text.length > 0 && (history.length === 0 || history[history.length - 1] !== text))
            history = [...history, text];
        historyIndex = -1;
        promptInput.text = "";
        clearImages();
        promptInput.forceActiveFocus();
    }

    function recallOlder() {
        if (history.length === 0) return;
        if (historyIndex === -1) {
            draft = promptInput.text;
            historyIndex = history.length - 1;
        } else if (historyIndex > 0) {
            historyIndex--;
        }
        promptInput.text = history[historyIndex];
        promptInput.cursorPosition = promptInput.text.length;
    }

    function recallNewer() {
        if (historyIndex === -1) return;
        if (historyIndex < history.length - 1) {
            historyIndex++;
            promptInput.text = history[historyIndex];
        } else {
            historyIndex = -1;
            promptInput.text = draft;
        }
        promptInput.cursorPosition = promptInput.text.length;
    }

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
                    Accessible.name: i18n("Remove attachment")
                    QQC2.ToolTip.text: i18n("Remove")
                    QQC2.ToolTip.visible: hovered
                }
            }
        }
    }

    RowLayout {
        spacing: Kirigami.Units.smallSpacing

        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.minimumHeight: Kirigami.Units.gridUnit * 2
            // Grow with content up to ~6 lines, then scroll.
            Layout.maximumHeight: Kirigami.Units.gridUnit * 9
            implicitHeight: Math.min(promptInput.implicitHeight + Kirigami.Units.smallSpacing,
                                     Kirigami.Units.gridUnit * 9)
            QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

            QQC2.TextArea {
                id: promptInput
                placeholderText: commandBar.attachedImages.length > 0
                    ? i18n("Describe the screenshot (optional)… — Enter to send, Shift+Enter for a new line")
                    : i18n("Type a prompt… — Enter to send, Shift+Enter for a new line")
                focus: true
                wrapMode: TextEdit.Wrap
                Accessible.name: i18n("Prompt input")

                Keys.onReturnPressed: function(event) {
                    if (event.modifiers & Qt.ShiftModifier) {
                        event.accepted = false; // let a newline be inserted
                    } else {
                        event.accepted = true;
                        commandBar.submitPrompt();
                    }
                }
                Keys.onEnterPressed: function(event) {
                    if (event.modifiers & Qt.ShiftModifier) {
                        event.accepted = false;
                    } else {
                        event.accepted = true;
                        commandBar.submitPrompt();
                    }
                }
                Keys.onUpPressed: function(event) {
                    if (cursorPosition === 0 || text.length === 0) {
                        commandBar.recallOlder();
                        event.accepted = true;
                    } else {
                        event.accepted = false;
                    }
                }
                Keys.onDownPressed: function(event) {
                    if (commandBar.historyIndex !== -1 && cursorPosition === text.length) {
                        commandBar.recallNewer();
                        event.accepted = true;
                    } else {
                        event.accepted = false;
                    }
                }
            }
        }

        QQC2.ToolButton {
            icon.name: "mail-attachment-symbolic"
            onClicked: commandBar.attachRequested()
            Accessible.name: i18n("Attach screenshot")
            QQC2.ToolTip.text: i18n("Attach a screenshot")
            QQC2.ToolTip.visible: hovered
        }

        QQC2.ToolButton {
            icon.name: "document-send-symbolic"
            enabled: !commandBar.busy && (promptInput.text.trim().length > 0 || commandBar.attachedImages.length > 0)
            onClicked: commandBar.submitPrompt()
            Accessible.name: i18n("Send prompt")
            QQC2.ToolTip.text: commandBar.busy ? i18n("Agent is working…") : i18n("Send (Enter)")
            QQC2.ToolTip.visible: hovered
        }
    }

    function submitPrompt() {
        if (commandBar.busy)
            return;
        if (promptInput.text.trim().length === 0 && attachedImages.length === 0)
            return;
        commandBar.promptSubmitted(promptInput.text.trim(), attachedImages.slice());
        // Note: text is cleared by acceptSubmit() once the host accepts it.
    }
}
