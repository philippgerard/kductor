import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import org.kde.kductor

FormCard.FormCardPage {
    id: settingsPage

    title: i18n("Settings")

    FormCard.FormHeader {
        title: i18n("Agents")
    }

    FormCard.FormCard {
        FormCard.FormComboBoxDelegate {
            id: defaultModelField
            text: i18n("Default model")
            model: ["opus", "sonnet", "haiku"]
            currentIndex: model.indexOf(AgentManager.defaultModel)
            onCurrentValueChanged: AgentManager.defaultModel = currentValue
        }

        FormCard.FormSpinBoxDelegate {
            id: maxAgentsField
            label: i18n("Max concurrent agents")
            from: 1
            to: 32
            value: AgentManager.maxConcurrentAgents
            onValueChanged: AgentManager.maxConcurrentAgents = value
        }
    }

    FormCard.FormHeader {
        title: i18n("System")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            id: trayField
            text: i18n("Show in system tray")
            checked: AgentManager.showInTray
            onCheckedChanged: AgentManager.showInTray = checked
        }

        FormCard.FormSwitchDelegate {
            id: notifyField
            text: i18n("Notify on agent completion")
            checked: AgentManager.notifyOnComplete
            onCheckedChanged: AgentManager.notifyOnComplete = checked
        }
    }

    FormCard.FormHeader {
        title: i18n("Info")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            text: i18n("Claude CLI")
            description: AgentManager.claudeAvailable ? AgentManager.claudePath() : i18n("Not found")
        }
    }
}
