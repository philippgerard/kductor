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

public:
    explicit AgentManager(QObject *parent = nullptr);

    Q_INVOKABLE QString createAgent(const QString &workspaceId);
    Q_INVOKABLE void startAgent(const QString &agentId, const QString &workingDir,
                                const QString &prompt, const QString &model = QStringLiteral("sonnet"));
    Q_INVOKABLE void sendPrompt(const QString &agentId, const QString &workingDir,
                                const QString &prompt, const QString &model = QStringLiteral("sonnet"));
    Q_INVOKABLE void stopAgent(const QString &agentId);
    Q_INVOKABLE void stopAll();
    Q_INVOKABLE void removeAgent(const QString &agentId);

    Q_INVOKABLE QStringList agentsForWorkspace(const QString &workspaceId) const;
    Q_INVOKABLE AgentOutputModel *outputModel(const QString &agentId) const;
    Q_INVOKABLE int agentStatus(const QString &agentId) const;
    Q_INVOKABLE QString agentActivity(const QString &agentId) const;

    Q_INVOKABLE bool canStartAgent() const;
    Q_INVOKABLE QString claudePath() const;

    int activeCount() const;
    bool claudeAvailable() const { return m_claudeAvailable; }

    int maxConcurrentAgents() const { return m_maxConcurrentAgents; }
    void setMaxConcurrentAgents(int max);

Q_SIGNALS:
    void activeCountChanged();
    void maxConcurrentAgentsChanged();
    void agentSpawned(const QString &agentId, const QString &workspaceId);
    void agentFinished(const QString &agentId);
    void agentError(const QString &agentId, const QString &error);

    void agentStatusChanged(const QString &agentId, int status);
    void agentActivityChanged(const QString &agentId, const QString &activity);
    void agentCostChanged(const QString &agentId, double cost);
    void agentOutput(const QString &agentId, int lineType, const QString &content, const QString &toolName);

private:
    QHash<QString, AgentProcess *> m_agents;
    QHash<QString, AgentOutputModel *> m_outputModels;
    QHash<QString, QString> m_agentToWorkspace;
    int m_maxConcurrentAgents = 8;
    bool m_claudeAvailable = false;
    QString m_claudePath;

    void connectAgent(const QString &agentId, AgentProcess *agent);
    void detectClaude();
};
