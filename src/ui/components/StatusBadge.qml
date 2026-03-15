import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

RowLayout {
    id: badge
    spacing: Kirigami.Units.smallSpacing

    // Maps to Workspace::Status and AgentProcess::Status
    property int status: 0

    readonly property var statusColors: [
        Kirigami.Theme.positiveTextColor,   // 0: Active/Idle
        Kirigami.Theme.neutralTextColor,    // 1: Paused/Starting
        Kirigami.Theme.focusColor,          // 2: Completed/Running
        Kirigami.Theme.disabledTextColor,   // 3: Archived/Completed
        Kirigami.Theme.negativeTextColor    // 4: Error
    ]

    readonly property var statusLabels: [
        i18n("Active"),
        i18n("Starting"),
        i18n("Running"),
        i18n("Completed"),
        i18n("Error")
    ]

    Rectangle {
        width: Kirigami.Units.smallSpacing * 2
        height: width
        radius: width / 2
        color: statusColors[Math.min(status, statusColors.length - 1)]

        SequentialAnimation on opacity {
            running: status === 2 // Running
            loops: Animation.Infinite
            NumberAnimation { to: 0.3; duration: 800; easing.type: Easing.InOutQuad }
            NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
        }
    }

    QQC2.Label {
        text: statusLabels[Math.min(status, statusLabels.length - 1)]
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        color: statusColors[Math.min(status, statusColors.length - 1)]
    }
}
