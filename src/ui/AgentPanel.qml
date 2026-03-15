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

        QQC2.ToolButton {
            icon.name: "media-playback-stop-symbolic"
            visible: agentPanel.agentStatus === 2
            flat: true
            onClicked: AgentManager.stopAgent(agentPanel.agentId)
            QQC2.ToolTip.text: i18n("Stop agent")
            QQC2.ToolTip.visible: hovered
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

    // Command bar
    CommandBar {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        Layout.topMargin: Kirigami.Units.smallSpacing
        Layout.bottomMargin: Kirigami.Units.smallSpacing
        onPromptSubmitted: function(prompt) {
            outputModel.appendSystem(prompt);
            if (agentPanel.agentStatus === 0) {
                AgentManager.startAgent(agentPanel.agentId, agentPanel.workingDir, prompt, "sonnet");
            } else {
                AgentManager.sendPrompt(agentPanel.agentId, agentPanel.workingDir, prompt, "sonnet");
            }
        }
    }
}
