import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// Small icon button that copies a string to the clipboard. Uses a hidden,
// off-screen TextEdit because QtQuick exposes the clipboard through TextEdit.copy().
QQC2.ToolButton {
    id: copyButton

    property string textToCopy: ""
    property string tooltip: i18n("Copy")

    icon.name: "edit-copy-symbolic"
    display: QQC2.AbstractButton.IconOnly
    flat: true
    enabled: textToCopy.length > 0

    Accessible.name: tooltip
    QQC2.ToolTip.text: tooltip
    QQC2.ToolTip.visible: hovered

    onClicked: {
        clip.text = copyButton.textToCopy;
        clip.selectAll();
        clip.copy();
        clip.text = "";
        applicationWindow().showPassiveNotification(i18n("Copied to clipboard"), 1500);
    }

    TextEdit {
        id: clip
        visible: false
        width: 0
        height: 0
    }
}
