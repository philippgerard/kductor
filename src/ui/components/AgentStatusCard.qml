import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.ItemDelegate {
    id: card

    property string agentId: ""
    property int agentIndex: 0
    property int agentStatus: 0
    property string activity: ""
    property double cost: 0.0

    contentItem: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        ColumnLayout {
            spacing: 2
            Layout.fillWidth: true

            QQC2.Label {
                text: i18n("Agent %1", agentIndex + 1)
                font.bold: true
            }

            QQC2.Label {
                text: activity
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        StatusBadge {
            status: agentStatus
        }
    }
}
