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
            textRole: "label"
            valueRole: "value"
            model: [
                { label: i18n("Fable 5"), value: "fable" },
                { label: i18n("Opus 4.6"), value: "opus" },
                { label: i18n("Sonnet 4.6"), value: "sonnet" },
                { label: i18n("Haiku 4.5"), value: "haiku" }
            ]
            currentIndex: model.findIndex(entry => entry.value === AgentManager.defaultModel)
            onCurrentValueChanged: AgentManager.defaultModel = currentValue
        }

        FormCard.FormComboBoxDelegate {
            id: permissionModeField
            text: i18n("Agent permissions")
            description: i18n("\"Full access\" lets agents run any command without confirmation.")
            textRole: "label"
            valueRole: "value"
            model: [
                { label: i18n("Full access — skip all permission prompts"), value: "bypass" },
                { label: i18n("Auto mode — approve actions automatically"), value: "auto" },
                { label: i18n("Auto-accept file edits"), value: "acceptEdits" },
                { label: i18n("Ask each time (default)"), value: "default" },
                { label: i18n("Plan mode (read-only)"), value: "plan" }
            ]
            currentIndex: model.findIndex(entry => entry.value === AgentManager.permissionMode)
            onCurrentValueChanged: AgentManager.permissionMode = currentValue
        }

        FormCard.FormTextFieldDelegate {
            id: extraFlagsField
            label: i18n("Extra CLI flags")
            placeholderText: i18n("e.g. --append-system-prompt \"be concise\"")
            text: AgentManager.extraFlags
            onTextChanged: AgentManager.extraFlags = text
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

        FormCard.FormTextDelegate {
            visible: !AgentManager.claudeAvailable
            text: i18n("Install hint")
            description: i18n("npm install -g @anthropic-ai/claude-code")
        }

        FormCard.FormButtonDelegate {
            id: recheckClaudeButton
            text: i18n("Check again")
            icon.name: "view-refresh"
            onClicked: AgentManager.redetectClaude()
        }
    }
}
