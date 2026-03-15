#include "agentmanager.h"
#include "agentprocess.h"

AgentManager::AgentManager(QObject *parent)
    : QObject(parent)
{
}

QString AgentManager::spawnAgent(const QString &workspaceId, const QString &workingDir,
                                 const QString &prompt, const QString &model)
{
    auto *agent = new AgentProcess(this);
    QString agentId = agent->agentId();

    m_agents.insert(agentId, agent);
    m_agentToWorkspace.insert(agentId, workspaceId);

    connect(agent, &AgentProcess::processFinished, this, [this, agentId](int) {
        Q_EMIT agentFinished(agentId);
        Q_EMIT activeCountChanged();
    });

    connect(agent, &AgentProcess::errorOccurred, this, [this, agentId](const QString &error) {
        Q_EMIT agentError(agentId, error);
    });

    agent->start(workingDir, prompt, model);

    Q_EMIT agentSpawned(agentId, workspaceId);
    Q_EMIT activeCountChanged();

    return agentId;
}

void AgentManager::stopAgent(const QString &agentId)
{
    auto it = m_agents.find(agentId);
    if (it != m_agents.end()) {
        (*it)->stop();
        Q_EMIT activeCountChanged();
    }
}

void AgentManager::stopAll()
{
    for (auto *agent : m_agents) {
        agent->stop();
    }
    Q_EMIT activeCountChanged();
}

void AgentManager::sendPrompt(const QString &agentId, const QString &workingDir,
                               const QString &prompt, const QString &model)
{
    auto it = m_agents.find(agentId);
    if (it != m_agents.end()) {
        (*it)->resume(workingDir, prompt, model);
    }
}

AgentProcess *AgentManager::getAgent(const QString &agentId) const
{
    return m_agents.value(agentId, nullptr);
}

QStringList AgentManager::agentsForWorkspace(const QString &workspaceId) const
{
    QStringList result;
    for (auto it = m_agentToWorkspace.cbegin(); it != m_agentToWorkspace.cend(); ++it) {
        if (it.value() == workspaceId) {
            result.append(it.key());
        }
    }
    return result;
}

int AgentManager::activeCount() const
{
    int count = 0;
    for (const auto *agent : m_agents) {
        if (agent->status() == AgentProcess::Running || agent->status() == AgentProcess::Starting) {
            ++count;
        }
    }
    return count;
}
