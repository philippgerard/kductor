import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kductor

ColumnLayout {
    id: agentPanel

    required property string agentId
    required property string workingDir

    property int agentStatus: AgentManager.agentStatus(agentId)
    property string agentActivity: AgentManager.agentActivity(agentId)
    property var outputModel: AgentManager.outputModel(agentId)
    property int contextUsed: 0
    property int contextWindow: 0
    property double agentCost: AgentManager.agentCost(agentId)

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
        function onAgentCostChanged(id, cost) {
            if (id === agentPanel.agentId)
                agentPanel.agentCost = cost;
        }
    }

    // Keyboard-accessible alternative to drag-and-drop screenshot attachment.
    FileDialog {
        id: attachDialog
        title: i18n("Attach Screenshots")
        fileMode: FileDialog.OpenFiles
        nameFilters: [i18n("Images (*.png *.jpg *.jpeg *.gif *.bmp *.webp *.svg)")]
        onAccepted: {
            for (let i = 0; i < selectedFiles.length; i++) {
                let path = decodeURIComponent(selectedFiles[i].toString().replace("file://", ""));
                commandBar.addImage(path);
            }
            commandBar.focusInput();
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
                source: "image-x-generic"
                Layout.preferredWidth: Kirigami.Units.iconSizes.huge
                Layout.preferredHeight: Kirigami.Units.iconSizes.huge
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

        // Current activity (what the agent is doing right now)
        QQC2.Label {
            visible: agentPanel.agentStatus === 2 && agentPanel.agentActivity.length > 0
            text: agentPanel.agentActivity
            elide: Text.ElideRight
            Layout.maximumWidth: Kirigami.Units.gridUnit * 16
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            opacity: 0.7
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
            opacity: 0.5
            QQC2.ToolTip.text: i18n("Context window used")
            QQC2.ToolTip.visible: hovered
        }

        // Cumulative cost for this agent
        QQC2.Label {
            visible: agentPanel.agentCost > 0
            text: "$" + agentPanel.agentCost.toFixed(agentPanel.agentCost < 1 ? 3 : 2)
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            opacity: 0.5
            QQC2.ToolTip.text: i18n("Total cost of this agent session")
            QQC2.ToolTip.visible: hovered
        }

        Item { Layout.fillWidth: true }

        QQC2.ComboBox {
            id: modelPicker
            textRole: "label"
            valueRole: "value"
            implicitWidth: Kirigami.Units.gridUnit * 8
            Accessible.name: i18n("Model")
            QQC2.ToolTip.text: i18n("Model for this agent")
            QQC2.ToolTip.visible: hovered
            model: [
                {label: "Fable 5",    value: "fable"},
                {label: "Opus 4.6",   value: "opus"},
                {label: "Sonnet 4.6", value: "sonnet"},
                {label: "Haiku 4.5",  value: "haiku"}
            ]
            Component.onCompleted: {
                let idx = model.findIndex(m => m.value === AgentManager.defaultModel);
                currentIndex = idx >= 0 ? idx : 1;
            }
        }

        QQC2.ToolButton {
            icon.name: "media-playback-stop-symbolic"
            // Allow stopping while Starting (1) or Running (2).
            visible: agentPanel.agentStatus === 1 || agentPanel.agentStatus === 2
            flat: true
            onClicked: AgentManager.stopAgent(agentPanel.agentId)
            Accessible.name: i18n("Stop agent")
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
                    closeIdleDialog.open();
                }
            }
            Accessible.name: i18n("Close agent")
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

    Kirigami.PromptDialog {
        id: closeIdleDialog
        title: i18n("Close Agent")
        subtitle: i18n("Closing this agent permanently deletes its conversation and output history. Continue?")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Close Agent")
                icon.name: "tab-close-symbolic"
                onTriggered: {
                    closeIdleDialog.close();
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
        // Disable sending while the agent is Starting/Running so prompts are
        // never silently dropped.
        busy: agentPanel.agentStatus === 1 || agentPanel.agentStatus === 2
        onAttachRequested: attachDialog.open()
        onPromptSubmitted: function(prompt, imagePaths) {
            if (!AgentManager.claudeAvailable) {
                if (agentPanel.outputModel)
                    agentPanel.outputModel.appendError(i18n("Claude CLI not found. Install it with: npm install -g @anthropic-ai/claude-code"));
                return; // keep the typed prompt
            }

            // A previously-run agent (e.g. restored after restart) has a session
            // to resume even while Idle; a brand-new agent starts fresh.
            let hasSession = AgentManager.agentSessionId(agentPanel.agentId).length > 0;
            let fresh = agentPanel.agentStatus === 0 && !hasSession;

            if (fresh && !AgentManager.canStartAgent()) {
                if (agentPanel.outputModel)
                    agentPanel.outputModel.appendError(i18n("Max concurrent agents reached (%1). Stop an agent first.", AgentManager.maxConcurrentAgents));
                return; // keep the typed prompt
            }

            let ok = fresh
                ? AgentManager.startAgent(agentPanel.agentId, agentPanel.workingDir, prompt, modelPicker.currentValue, imagePaths)
                : AgentManager.sendPrompt(agentPanel.agentId, agentPanel.workingDir, prompt, modelPicker.currentValue, imagePaths);

            if (!ok) {
                if (agentPanel.outputModel)
                    agentPanel.outputModel.appendError(i18n("Agent is busy — wait for it to finish before sending."));
                return; // keep the typed prompt
            }

            // Echo the message only once it was actually accepted.
            let displayText = prompt;
            if (imagePaths.length > 0) {
                let names = imagePaths.map(function(p) { return p.split('/').pop(); });
                let prefix = i18n("Attached: %1", names.join(", "));
                displayText = prompt.length > 0 ? prefix + "\n" + prompt : prefix;
            }
            if (agentPanel.outputModel)
                agentPanel.outputModel.appendSystem(displayText);
            commandBar.acceptSubmit();
        }
    }
}
