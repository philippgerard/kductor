import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    title: i18n("Kductor")
    minimumWidth: Kirigami.Units.gridUnit * 40
    minimumHeight: Kirigami.Units.gridUnit * 30
    width: Kirigami.Units.gridUnit * 60
    height: Kirigami.Units.gridUnit * 40

    globalDrawer: Kirigami.GlobalDrawer {
        title: i18n("Kductor")
        titleIcon: "kductor"
        isMenu: true
        actions: [
            Kirigami.Action {
                text: i18n("Dashboard")
                icon.name: "go-home-symbolic"
                onTriggered: {
                    while (pageStack.depth > 1)
                        pageStack.pop();
                }
            },
            Kirigami.Action {
                text: i18n("Settings")
                icon.name: "configure"
                onTriggered: pageStack.pushDialogLayer(settingsPage)
            },
            Kirigami.Action {
                text: i18n("About Kductor")
                icon.name: "help-about"
                onTriggered: pageStack.pushDialogLayer(aboutPage)
            }
        ]
    }

    Component {
        id: settingsPage
        SettingsPage {}
    }

    Component {
        id: aboutPage
        AboutPage {}
    }

    pageStack.columnView.columnResizeMode: Kirigami.ColumnView.SingleColumn
    pageStack.initialPage: DashboardPage {}
}
