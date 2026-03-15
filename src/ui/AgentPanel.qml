import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kductor

ColumnLayout {
    id: agentPanel

    required property string agentId
    required property string workingDir

    property var agent: agentId ? AgentManager.getAgent(agentId) : null

    AgentOutputModel {
        id: outputModel
    }

    Connections {
        target: agent
        enabled: agent !== null

        function onAssistantText(text) {
            outputModel.appendText(text);
        }
        function onThinkingText(thought) {
            outputModel.appendThinking(thought);
        }
        function onToolUse(toolName, input) {
            let summary = toolName;
            if (input && input.command) {
                summary += ": " + input.command;
            } else if (input && input.pattern) {
                summary += ": " + input.pattern;
            } else if (input && input.file_path) {
                summary += ": " + input.file_path;
            }
            outputModel.appendToolUse(toolName, summary);
        }
        function onToolResult(toolName, output) {
            outputModel.appendToolResult(toolName, output);
        }
        function onErrorOccurred(error) {
            outputModel.appendError(error);
        }
        function onInitialized(info) {
            outputModel.appendSystem(i18n("Agent initialized (session: %1)", info.session_id || ""));
        }
        function onResultReady(result) {
            outputModel.appendSystem(i18n("Agent completed. Cost: $%1", agent.totalCost.toFixed(4)));
        }
    }

    // Status bar
    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.smallSpacing

        StatusBadge {
            status: agent ? agent.status : 0
        }

        QQC2.Label {
            text: agent ? agent.currentActivity : i18n("Idle")
            elide: Text.ElideRight
            Layout.fillWidth: true
            opacity: 0.7
            font.italic: true
        }

        QQC2.Label {
            visible: agent && agent.totalCost > 0
            text: agent ? "$" + agent.totalCost.toFixed(4) : ""
            opacity: 0.6
            font.family: "monospace"
        }

        QQC2.Button {
            icon.name: "media-playback-stop-symbolic"
            text: i18n("Stop")
            visible: agent && agent.status === 2 // Running
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

    // Command bar
    CommandBar {
        Layout.fillWidth: true
        enabled: agent !== null
        onPromptSubmitted: function(prompt) {
            AgentManager.sendPrompt(agentPanel.agentId, agentPanel.workingDir, prompt, "sonnet");
            outputModel.appendSystem(i18n("You: %1", prompt));
        }
    }
}
