#pragma once

#include <QObject>
#include <QHash>
#include <QString>

class AgentProcess;
class AgentOutputModel;

class AgentManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int activeCount READ activeCount NOTIFY activeCountChanged)
    Q_PROPERTY(int maxConcurrentAgents READ maxConcurrentAgents WRITE setMaxConcurrentAgents NOTIFY maxConcurrentAgentsChanged)
    Q_PROPERTY(bool claudeAvailable READ claudeAvailable CONSTANT)
    Q_PROPERTY(QString defaultModel READ defaultModel WRITE setDefaultModel NOTIFY defaultModelChanged)
    Q_PROPERTY(bool showInTray READ showInTray WRITE setShowInTray NOTIFY showInTrayChanged)
    Q_PROPERTY(bool notifyOnComplete READ notifyOnComplete WRITE setNotifyOnComplete NOTIFY notifyOnCompleteChanged)

public:
    explicit AgentManager(QObject *parent = nullptr);

    Q_INVOKABLE QString createAgent(const QString &workspaceId, const QString &name = QString());
    Q_INVOKABLE void startAgent(const QString &agentId, const QString &workingDir,
                                const QString &prompt, const QString &model = QStringLiteral("sonnet"),
                                const QStringList &imagePaths = {});
    Q_INVOKABLE void sendPrompt(const QString &agentId, const QString &workingDir,
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

    Q_INVOKABLE bool canStartAgent() const;
    Q_INVOKABLE int workspaceAgentStatus(const QString &workspaceId) const; // 0=idle, 2=running
    Q_INVOKABLE QString claudePath() const;

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

Q_SIGNALS:
    void activeCountChanged();
    void maxConcurrentAgentsChanged();
    void defaultModelChanged();
    void showInTrayChanged();
    void notifyOnCompleteChanged();
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
    int m_maxConcurrentAgents = 8;
    bool m_claudeAvailable = false;
    QString m_claudePath;
    QString m_defaultModel = QStringLiteral("opus");
    bool m_showInTray = true;
    bool m_notifyOnComplete = true;

    void connectAgent(const QString &agentId, AgentProcess *agent);
    void detectGitEvent(const QString &agentId, const QString &command);
    void detectClaude();
    void saveAgents();
    void loadAgents();

    QHash<QString, QString> m_pendingGitCommands; // agentId -> last git-related Bash command
};
