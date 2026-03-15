import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.AbstractCard {
    id: card

    property string workspaceName: ""
    property string repositoryPath: ""
    property string branch: ""
    property int workspaceStatus: 0
    property int agents: 0

    signal clicked()

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true

            Kirigami.Heading {
                level: 3
                text: workspaceName
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            StatusBadge {
                status: workspaceStatus
            }
        }

        RowLayout {
            spacing: Kirigami.Units.smallSpacing
            Layout.fillWidth: true

            Kirigami.Icon {
                source: "vcs-branch"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                opacity: 0.7
            }
            QQC2.Label {
                text: branch
                opacity: 0.7
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        RowLayout {
            spacing: Kirigami.Units.smallSpacing
            Layout.fillWidth: true

            Kirigami.Icon {
                source: "folder-symbolic"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                opacity: 0.7
            }
            QQC2.Label {
                text: {
                    let parts = repositoryPath.split("/");
                    return parts[parts.length - 1] || repositoryPath;
                }
                opacity: 0.7
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true

            Kirigami.Icon {
                source: "system-run-symbolic"
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Kirigami.Units.iconSizes.small
                opacity: 0.7
            }
            QQC2.Label {
                text: i18np("%1 agent", "%1 agents", agents)
                opacity: 0.7
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
            Item { Layout.fillWidth: true }
        }
    }

    onClicked: card.clicked()

    HoverHandler {
        id: hoverHandler
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: card.clicked()
    }

    background: Kirigami.ShadowedRectangle {
        color: hoverHandler.hovered ? Kirigami.Theme.highlightColor.alpha(0.1) : Kirigami.Theme.backgroundColor
        radius: Kirigami.Units.mediumSpacing
        border.width: 1
        border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.15)

        shadow.size: hoverHandler.hovered ? Kirigami.Units.largeSpacing : Kirigami.Units.smallSpacing
        shadow.color: Qt.rgba(0, 0, 0, 0.1)
        shadow.yOffset: 2

        Behavior on shadow.size {
            NumberAnimation { duration: Kirigami.Units.shortDuration }
        }
        Behavior on color {
            ColorAnimation { duration: Kirigami.Units.shortDuration }
        }
    }
}
