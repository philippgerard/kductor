import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

RowLayout {
    id: badge
    spacing: Kirigami.Units.smallSpacing

    property int status: 0
    property string context: "workspace" // "workspace" or "agent"

    readonly property var colors: [
        Kirigami.Theme.positiveTextColor,   // 0
        Kirigami.Theme.neutralTextColor,    // 1
        Kirigami.Theme.focusColor,          // 2
        Kirigami.Theme.disabledTextColor,   // 3
        Kirigami.Theme.negativeTextColor    // 4
    ]

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
        color: colors[Math.min(status, colors.length - 1)]

        SequentialAnimation on opacity {
            running: pulsing
            loops: Animation.Infinite
            NumberAnimation { to: 0.3; duration: 800; easing.type: Easing.InOutQuad }
            NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
        }
    }

    QQC2.Label {
        text: labels[Math.min(status, labels.length - 1)]
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        color: colors[Math.min(status, colors.length - 1)]
    }
}
