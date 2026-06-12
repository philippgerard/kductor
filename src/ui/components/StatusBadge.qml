import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

RowLayout {
    id: badge
    spacing: Kirigami.Units.smallSpacing

    property int status: 0
    property string context: "workspace" // "workspace" or "agent"

    readonly property var workspaceColors: [
        Kirigami.Theme.positiveTextColor,   // 0 Active
        Kirigami.Theme.neutralTextColor,    // 1 Paused
        Kirigami.Theme.focusColor,          // 2 Completed
        Kirigami.Theme.disabledTextColor,   // 3 Archived
        Kirigami.Theme.negativeTextColor    // 4 Error
    ]

    readonly property var agentColors: [
        Kirigami.Theme.disabledTextColor,   // 0 Idle
        Kirigami.Theme.neutralTextColor,    // 1 Starting
        Kirigami.Theme.focusColor,          // 2 Running
        Kirigami.Theme.positiveTextColor,   // 3 Completed
        Kirigami.Theme.negativeTextColor    // 4 Error
    ]

    readonly property var activeColors: context === "agent" ? agentColors : workspaceColors

    readonly property var workspaceLabels: [
        i18n("Active"),
        i18n("Paused"),
        i18n("Completed"),
        i18n("Archived"),
        i18n("Error")
    ]

    readonly property var agentLabels: [
        i18n("Idle"),
        i18n("Starting"),
        i18n("Running"),
        i18n("Completed"),
        i18n("Error")
    ]

    readonly property var labels: context === "agent" ? agentLabels : workspaceLabels
    readonly property bool pulsing: (context === "agent" && status === 2)

    Rectangle {
        width: Kirigami.Units.smallSpacing * 2
        height: width
        radius: width / 2
        color: activeColors[Math.min(status, activeColors.length - 1)]

        SequentialAnimation on opacity {
            // Kirigami collapses durations to 1 when animations are disabled
            // (reduced motion); skip the pulse in that case.
            running: pulsing && Kirigami.Units.longDuration > 1
            loops: Animation.Infinite
            NumberAnimation { to: 0.3; duration: Kirigami.Units.longDuration * 4; easing.type: Easing.InOutQuad }
            NumberAnimation { to: 1.0; duration: Kirigami.Units.longDuration * 4; easing.type: Easing.InOutQuad }
        }
    }

    QQC2.Label {
        text: labels[Math.min(status, labels.length - 1)]
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        color: Kirigami.Theme.textColor
    }
}
