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

    readonly property var imageExtensions: ["png", "jpg", "jpeg", "gif", "bmp", "webp", "svg"]

    function isImageFile(path) {
        let ext = path.split('.').pop().toLowerCase();
        return imageExtensions.indexOf(ext) !== -1;
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
        function onAgentContextUpdated(id, used, window) {
            if (id === agentPanel.agentId) {
                agentPanel.contextUsed = used;
                agentPanel.contextWindow = window;
            }
        }
    }

    // Drop overlay — covers entire panel
    DropArea {
        id: dropArea
        anchors.fill: parent
        z: 1
        keys: ["text/uri-list"]

        onEntered: function(drag) {
            if (drag.hasUrls) {
                let hasImage = false;
                for (let i = 0; i < drag.urls.length; i++) {
                    let url = drag.urls[i].toString();
                    if (url.startsWith("file://") && agentPanel.isImageFile(url)) {
                        hasImage = true;
                        break;
                    }
                }
                if (hasImage) {
                    drag.accepted = true;
                    dropOverlay.visible = true;
                } else {
                    drag.accepted = false;
                }
            }
        }

        onExited: {
            dropOverlay.visible = false;
        }

        onDropped: function(drop) {
            dropOverlay.visible = false;
            if (drop.hasUrls) {
                for (let i = 0; i < drop.urls.length; i++) {
                    let url = drop.urls[i].toString();
                    if (url.startsWith("file://") && agentPanel.isImageFile(url)) {
                        let path = url.replace("file://", "");
                        path = decodeURIComponent(path);
                        commandBar.addImage(path);
                    }
                }
                commandBar.focusInput();
            }
        }
    }

    // Visual drop feedback
    Rectangle {
        id: dropOverlay
        anchors.fill: parent
        z: 2
        visible: false
        color: Qt.rgba(Kirigami.Theme.highlightColor.r,
                       Kirigami.Theme.highlightColor.g,
                       Kirigami.Theme.highlightColor.b, 0.15)
        border.color: Kirigami.Theme.highlightColor
        border.width: 2
        radius: Kirigami.Units.cornerRadius

        ColumnLayout {
            anchors.centerIn: parent
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Icon {
                Layout.alignment: Qt.AlignHCenter
                source: "image-x-generic-symbolic"
                width: Kirigami.Units.iconSizes.huge
                height: width
                color: Kirigami.Theme.highlightColor
            }
            QQC2.Label {
                Layout.alignment: Qt.AlignHCenter
                text: i18n("Drop screenshots here")
                font.bold: true
                color: Kirigami.Theme.highlightColor
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
        id: commandBar
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        Layout.topMargin: Kirigami.Units.smallSpacing
        Layout.bottomMargin: Kirigami.Units.smallSpacing
        onPromptSubmitted: function(prompt, imagePaths) {
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

            // Build display text
            let displayText = prompt;
            if (imagePaths.length > 0) {
                let names = imagePaths.map(function(p) { return p.split('/').pop(); });
                let prefix = "📎 " + names.join(", ");
                displayText = prompt.length > 0 ? prefix + "\n" + prompt : prefix;
            }
            if (agentPanel.outputModel)
                agentPanel.outputModel.appendSystem(displayText);

            if (agentPanel.agentStatus === 0) {
                AgentManager.startAgent(agentPanel.agentId, agentPanel.workingDir, prompt, modelPicker.currentValue, imagePaths);
            } else {
                AgentManager.sendPrompt(agentPanel.agentId, agentPanel.workingDir, prompt, modelPicker.currentValue, imagePaths);
            }
        }
    }
}
