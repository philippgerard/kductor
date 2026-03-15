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

    Q_INVOKABLE QString spawnAgent(const QString &workspaceId, const QString &workingDir,
                                   const QString &prompt, const QString &model = QStringLiteral("sonnet"));
    Q_INVOKABLE void stopAgent(const QString &agentId);
    Q_INVOKABLE void stopAll();
    Q_INVOKABLE void sendPrompt(const QString &agentId, const QString &workingDir,
                                const QString &prompt, const QString &model = QStringLiteral("sonnet"));

    Q_INVOKABLE AgentProcess *getAgent(const QString &agentId) const;
    Q_INVOKABLE QStringList agentsForWorkspace(const QString &workspaceId) const;

    int activeCount() const;

Q_SIGNALS:
    void activeCountChanged();
    void agentSpawned(const QString &agentId, const QString &workspaceId);
    void agentFinished(const QString &agentId);
    void agentError(const QString &agentId, const QString &error);

private:
    QHash<QString, AgentProcess *> m_agents;
    QHash<QString, QString> m_agentToWorkspace; // agentId -> workspaceId
};
