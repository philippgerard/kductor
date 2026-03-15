import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kductor

ColumnLayout {
    id: agentPanel

    required property string agentId
    required property string workingDir

    property int agentStatus: AgentManager.agentStatus(agentId)
    property string agentActivity: ""
    property var outputModel: AgentManager.outputModel(agentId)
    property int contextUsed: 0
    property int contextWindow: 0

    signal closeRequested(string agentId)

    Connections {
        target: AgentManager

        function onAgentStatusChanged(id, status) {
            if (id === agentPanel.agentId)
                agentPanel.agentStatus = status;
        }
        function onAgentActivityChanged(id, activity) {
            if (id === agentPanel.agentId)
                agentPanel.agentActivity = activity;
        }
        function onAgentContextUpdated(id, used, window) {
            if (id === agentPanel.agentId) {
                agentPanel.contextUsed = used;
                agentPanel.contextWindow = window;
            }
        }
    }

    // Status bar — compact
    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        Layout.topMargin: Kirigami.Units.smallSpacing
        Layout.bottomMargin: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        StatusBadge {
            context: "agent"
            status: agentPanel.agentStatus
        }

        // Context usage
        QQC2.Label {
            visible: contextUsed > 0
            text: {
                let usedK = (contextUsed / 1000).toFixed(1);
                if (contextWindow > 0) {
                    let totalK = Math.round(contextWindow / 1000);
                    let pct = Math.round(contextUsed / contextWindow * 100);
                    return usedK + "k / " + totalK + "k (" + pct + "%)";
                }
                return usedK + "k tokens";
            }
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            opacity: 0.4
        }

        Item { Layout.fillWidth: true }

        QQC2.ComboBox {
            id: modelPicker
            textRole: "label"
            valueRole: "value"
            implicitWidth: Kirigami.Units.gridUnit * 8
            model: [
                {label: "Opus 4.6",   value: "opus"},
                {label: "Sonnet 4.6", value: "sonnet"},
                {label: "Haiku 4.5",  value: "haiku"}
            ]
            currentIndex: 0
        }

        QQC2.ToolButton {
            icon.name: "media-playback-stop-symbolic"
            visible: agentPanel.agentStatus === 2
            flat: true
            onClicked: AgentManager.stopAgent(agentPanel.agentId)
            QQC2.ToolTip.text: i18n("Stop agent")
            QQC2.ToolTip.visible: hovered
        }

        QQC2.ToolButton {
            icon.name: "tab-close-symbolic"
            flat: true
            onClicked: {
                if (agentPanel.agentStatus === 1 || agentPanel.agentStatus === 2) {
                    closeAgentDialog.open();
                } else {
                    agentPanel.closeRequested(agentPanel.agentId);
                }
            }
            QQC2.ToolTip.text: i18n("Close agent")
            QQC2.ToolTip.visible: hovered
        }
    }

    Kirigami.PromptDialog {
        id: closeAgentDialog
        title: i18n("Agent Running")
        subtitle: i18n("This agent is still running. Stop and close it?")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Stop & Close")
                icon.name: "tab-close-symbolic"
                onTriggered: {
                    closeAgentDialog.close();
                    agentPanel.closeRequested(agentPanel.agentId);
                }
            }
        ]
    }

    Kirigami.Separator {
        Layout.fillWidth: true
    }

    // Output area — bound to persistent model from C++
    StreamingTextArea {
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: agentPanel.outputModel
    }

    Kirigami.Separator {
        Layout.fillWidth: true
    }

    // Command bar
    CommandBar {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        Layout.topMargin: Kirigami.Units.smallSpacing
        Layout.bottomMargin: Kirigami.Units.smallSpacing
        onPromptSubmitted: function(prompt) {
            if (!AgentManager.claudeAvailable) {
                if (agentPanel.outputModel)
                    agentPanel.outputModel.appendError(i18n("Claude CLI not found. Install it with: npm install -g @anthropic-ai/claude-code"));
                return;
            }
            if (agentPanel.agentStatus === 0 && !AgentManager.canStartAgent()) {
                if (agentPanel.outputModel)
                    agentPanel.outputModel.appendError(i18n("Max concurrent agents reached (%1). Stop an agent first.", AgentManager.maxConcurrentAgents));
                return;
            }
            if (agentPanel.outputModel)
                agentPanel.outputModel.appendSystem(prompt);
            if (agentPanel.agentStatus === 0) {
                AgentManager.startAgent(agentPanel.agentId, agentPanel.workingDir, prompt, modelPicker.currentValue);
            } else {
                AgentManager.sendPrompt(agentPanel.agentId, agentPanel.workingDir, prompt, modelPicker.currentValue);
            }
        }
    }
}
