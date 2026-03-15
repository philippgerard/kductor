#pragma once

#include <QObject>
#include <QHash>
#include <QString>

class AgentProcess;

class AgentManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int activeCount READ activeCount NOTIFY activeCountChanged)

public:
    explicit AgentManager(QObject *parent = nullptr);

    Q_INVOKABLE QString createAgent(const QString &workspaceId);
    Q_INVOKABLE void startAgent(const QString &agentId, const QString &workingDir,
                                const QString &prompt, const QString &model = QStringLiteral("sonnet"));
    Q_INVOKABLE void sendPrompt(const QString &agentId, const QString &workingDir,
                                const QString &prompt, const QString &model = QStringLiteral("sonnet"));
    Q_INVOKABLE void stopAgent(const QString &agentId);
    Q_INVOKABLE void stopAll();

    Q_INVOKABLE int agentStatus(const QString &agentId) const;
    Q_INVOKABLE QString agentActivity(const QString &agentId) const;
    Q_INVOKABLE double agentCost(const QString &agentId) const;

    int activeCount() const;

Q_SIGNALS:
    void activeCountChanged();
    void agentSpawned(const QString &agentId, const QString &workspaceId);
    void agentFinished(const QString &agentId);
    void agentError(const QString &agentId, const QString &error);

    // Forwarded from AgentProcess so QML can connect without the raw pointer
    void agentStatusChanged(const QString &agentId, int status);
    void agentActivityChanged(const QString &agentId, const QString &activity);
    void agentCostChanged(const QString &agentId, double cost);
    void agentOutput(const QString &agentId, int lineType, const QString &content, const QString &toolName);

private:
    QHash<QString, AgentProcess *> m_agents;
    QHash<QString, QString> m_agentToWorkspace;

    void connectAgent(const QString &agentId, AgentProcess *agent);
};
