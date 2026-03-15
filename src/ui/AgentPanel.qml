import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kductor

ColumnLayout {
    id: agentPanel

    required property string agentId
    required property string workingDir

    property int agentStatus: 0
    property string agentActivity: ""
    property double agentCost: 0.0

    AgentOutputModel {
        id: outputModel
    }

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
        function onAgentCostChanged(id, cost) {
            if (id === agentPanel.agentId)
                agentPanel.agentCost = cost;
        }
        function onAgentOutput(id, lineType, content, toolName) {
            if (id !== agentPanel.agentId)
                return;
            switch (lineType) {
            case 0: outputModel.appendText(content); break;
            case 1: outputModel.appendThinking(content); break;
            case 2: outputModel.appendToolUse(toolName, content); break;
            case 3: outputModel.appendToolResult(toolName, content); break;
            case 4: outputModel.appendSystem(content); break;
            case 5: outputModel.appendError(content); break;
            }
        }
    }

    // Status bar
    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.smallSpacing

        StatusBadge {
            status: agentPanel.agentStatus
        }

        QQC2.Label {
            text: agentPanel.agentActivity || i18n("Idle")
            elide: Text.ElideRight
            Layout.fillWidth: true
            opacity: 0.7
            font.italic: true
        }

        QQC2.Label {
            visible: agentPanel.agentCost > 0
            text: "$" + agentPanel.agentCost.toFixed(4)
            opacity: 0.6
            font.family: "monospace"
        }

        QQC2.Button {
            icon.name: "media-playback-stop-symbolic"
            text: i18n("Stop")
            visible: agentPanel.agentStatus === 2 // Running
            flat: true
            onClicked: AgentManager.stopAgent(agentPanel.agentId)
        }
    }

    Kirigami.Separator {
        Layout.fillWidth: true
    }

    // Output area
    StreamingTextArea {
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: outputModel
    }

    Kirigami.Separator {
        Layout.fillWidth: true
    }

    // Command bar - always active
    CommandBar {
        Layout.fillWidth: true
        onPromptSubmitted: function(prompt) {
            outputModel.appendSystem(i18n("You: %1", prompt));
            if (agentPanel.agentStatus === 0) {
                // Idle - start the agent with this prompt
                AgentManager.startAgent(agentPanel.agentId, agentPanel.workingDir, prompt, "sonnet");
            } else {
                // Already started - send follow-up prompt
                AgentManager.sendPrompt(agentPanel.agentId, agentPanel.workingDir, prompt, "sonnet");
            }
        }
    }
}
