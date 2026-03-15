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
            status: agentPanel.agentStatus
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
            onClicked: agentPanel.closeRequested(agentPanel.agentId)
            QQC2.ToolTip.text: i18n("Close agent")
            QQC2.ToolTip.visible: hovered
        }
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
