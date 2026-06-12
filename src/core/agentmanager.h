#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

class AgentProcess;
class AgentOutputModel;

class AgentManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int activeCount READ activeCount NOTIFY activeCountChanged)
    Q_PROPERTY(int maxConcurrentAgents READ maxConcurrentAgents WRITE setMaxConcurrentAgents NOTIFY maxConcurrentAgentsChanged)
    Q_PROPERTY(bool claudeAvailable READ claudeAvailable NOTIFY claudeAvailableChanged)
    Q_PROPERTY(QString defaultModel READ defaultModel WRITE setDefaultModel NOTIFY defaultModelChanged)
    Q_PROPERTY(bool showInTray READ showInTray WRITE setShowInTray NOTIFY showInTrayChanged)
    Q_PROPERTY(bool notifyOnComplete READ notifyOnComplete WRITE setNotifyOnComplete NOTIFY notifyOnCompleteChanged)
    Q_PROPERTY(QString permissionMode READ permissionMode WRITE setPermissionMode NOTIFY permissionModeChanged)
    Q_PROPERTY(QString extraFlags READ extraFlags WRITE setExtraFlags NOTIFY extraFlagsChanged)

public:
    explicit AgentManager(QObject *parent = nullptr);

    Q_INVOKABLE QString createAgent(const QString &workspaceId, const QString &name = QString());
    // Return true if the prompt was actually delivered to a launched process.
    Q_INVOKABLE bool startAgent(const QString &agentId, const QString &workingDir,
                                const QString &prompt, const QString &model = QStringLiteral("sonnet"),
                                const QStringList &imagePaths = {});
    Q_INVOKABLE bool sendPrompt(const QString &agentId, const QString &workingDir,
                                const QString &prompt, const QString &model = QStringLiteral("sonnet"),
                                const QStringList &imagePaths = {});
    Q_INVOKABLE void stopAgent(const QString &agentId);
    Q_INVOKABLE void stopAll();
    Q_INVOKABLE void removeAgent(const QString &agentId);

    Q_INVOKABLE QStringList agentsForWorkspace(const QString &workspaceId) const;
    Q_INVOKABLE QString agentName(const QString &agentId) const;
    Q_INVOKABLE AgentOutputModel *outputModel(const QString &agentId) const;
    Q_INVOKABLE int agentStatus(const QString &agentId) const;
    Q_INVOKABLE QString agentActivity(const QString &agentId) const;
    Q_INVOKABLE QString agentSessionId(const QString &agentId) const;
    Q_INVOKABLE double agentCost(const QString &agentId) const;

    Q_INVOKABLE bool canStartAgent() const;
    Q_INVOKABLE int workspaceAgentStatus(const QString &workspaceId) const; // 0=idle, 2=running
    Q_INVOKABLE QString claudePath() const;
    // Re-run claude CLI detection (e.g. after the user installs it).
    Q_INVOKABLE bool redetectClaude();

    int activeCount() const;
    bool claudeAvailable() const { return m_claudeAvailable; }

    int maxConcurrentAgents() const { return m_maxConcurrentAgents; }
    void setMaxConcurrentAgents(int max);

    QString defaultModel() const { return m_defaultModel; }
    void setDefaultModel(const QString &model);

    bool showInTray() const { return m_showInTray; }
    void setShowInTray(bool show);

    bool notifyOnComplete() const { return m_notifyOnComplete; }
    void setNotifyOnComplete(bool notify);

    QString permissionMode() const { return m_permissionMode; }
    void setPermissionMode(const QString &mode);

    QString extraFlags() const { return m_extraFlags; }
    void setExtraFlags(const QString &flags);

Q_SIGNALS:
    void activeCountChanged();
    void maxConcurrentAgentsChanged();
    void defaultModelChanged();
    void showInTrayChanged();
    void notifyOnCompleteChanged();
    void permissionModeChanged();
    void extraFlagsChanged();
    void claudeAvailableChanged();
    void agentSpawned(const QString &agentId, const QString &workspaceId);
    void agentFinished(const QString &agentId);
    void agentError(const QString &agentId, const QString &error);

    void agentStatusChanged(const QString &agentId, int status);
    void agentActivityChanged(const QString &agentId, const QString &activity);
    void agentCostChanged(const QString &agentId, double cost);
    void agentOutput(const QString &agentId, int lineType, const QString &content, const QString &toolName);
    void agentContextUpdated(const QString &agentId, int used, int window);
    void gitEventDetected(const QString &workspaceId, const QString &eventType);

    friend class AgentManagerTest;

private:
    QHash<QString, AgentProcess *> m_agents;
    QHash<QString, AgentOutputModel *> m_outputModels;
    QHash<QString, QString> m_agentToWorkspace;
    QHash<QString, QString> m_agentNames;
    QStringList m_agentOrder; // stable creation/restore order for deterministic tabs
    int m_maxConcurrentAgents = 8;
    bool m_claudeAvailable = false;
    QString m_claudePath;
    QString m_defaultModel = QStringLiteral("opus");
    bool m_showInTray = true;
    bool m_notifyOnComplete = true;
    QString m_permissionMode = QStringLiteral("bypass");
    QString m_extraFlags;

    void connectAgent(const QString &agentId, AgentProcess *agent);
    void detectGitEvent(const QString &agentId, const QString &command);
    void detectClaude();
    void saveAgents();             // metadata + every agent's output (e.g. on quit)
    void saveAgentsMetadata();     // agents.json only — cheap, safe to call often
    void saveAgentOutput(const QString &agentId); // one transcript file
    void loadAgents();

    QHash<QString, QString> m_pendingGitCommands; // agentId -> last git-related Bash command
};
