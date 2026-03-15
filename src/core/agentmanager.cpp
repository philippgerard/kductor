#include "agentmanager.h"
#include "agentprocess.h"
#include "agentoutputmodel.h"

#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QStandardPaths>

AgentManager::AgentManager(QObject *parent)
    : QObject(parent)
{
    detectClaude();
}

void AgentManager::detectClaude()
{
    QStringList candidates = {
        QDir::homePath() + QStringLiteral("/.local/bin/claude"),
        QDir::homePath() + QStringLiteral("/.npm/bin/claude"),
        QStringLiteral("/usr/local/bin/claude"),
        QStringLiteral("/usr/bin/claude"),
    };
    for (const auto &path : candidates) {
        if (QFile::exists(path)) {
            m_claudePath = path;
            m_claudeAvailable = true;
            return;
        }
    }
    QString found = QStandardPaths::findExecutable(QStringLiteral("claude"));
    if (!found.isEmpty()) {
        m_claudePath = found;
        m_claudeAvailable = true;
    }
}

QString AgentManager::createAgent(const QString &workspaceId)
{
    auto *agent = new AgentProcess(this);
    auto *model = new AgentOutputModel(this);
    QString agentId = agent->agentId();

    m_agents.insert(agentId, agent);
    m_outputModels.insert(agentId, model);
    m_agentToWorkspace.insert(agentId, workspaceId);
    connectAgent(agentId, agent);

    Q_EMIT agentSpawned(agentId, workspaceId);
    Q_EMIT activeCountChanged();
    return agentId;
}

AgentOutputModel *AgentManager::outputModel(const QString &agentId) const
{
    return m_outputModels.value(agentId, nullptr);
}

void AgentManager::startAgent(const QString &agentId, const QString &workingDir,
                               const QString &prompt, const QString &model)
{
    auto *agent = m_agents.value(agentId, nullptr);
    if (!agent || !canStartAgent())
        return;
    agent->start(workingDir, prompt, model);
    Q_EMIT activeCountChanged();
}

void AgentManager::sendPrompt(const QString &agentId, const QString &workingDir,
                               const QString &prompt, const QString &model)
{
    auto *agent = m_agents.value(agentId, nullptr);
    if (!agent)
        return;
    agent->resume(workingDir, prompt, model);
    Q_EMIT activeCountChanged();
}

void AgentManager::stopAgent(const QString &agentId)
{
    auto *agent = m_agents.value(agentId, nullptr);
    if (agent) {
        agent->stop();
        Q_EMIT activeCountChanged();
    }
}

void AgentManager::stopAll()
{
    for (auto *agent : m_agents)
        agent->stop();
    Q_EMIT activeCountChanged();
}

void AgentManager::removeAgent(const QString &agentId)
{
    stopAgent(agentId);

    if (auto *agent = m_agents.take(agentId))
        agent->deleteLater();
    if (auto *model = m_outputModels.take(agentId))
        model->deleteLater();
    m_agentToWorkspace.remove(agentId);

    Q_EMIT activeCountChanged();
}

QStringList AgentManager::agentsForWorkspace(const QString &workspaceId) const
{
    QStringList result;
    for (auto it = m_agentToWorkspace.cbegin(); it != m_agentToWorkspace.cend(); ++it) {
        if (it.value() == workspaceId)
            result.append(it.key());
    }
    return result;
}

int AgentManager::agentStatus(const QString &agentId) const
{
    auto *agent = m_agents.value(agentId, nullptr);
    return agent ? agent->status() : 0;
}

QString AgentManager::agentActivity(const QString &agentId) const
{
    auto *agent = m_agents.value(agentId, nullptr);
    return agent ? agent->currentActivity() : QString();
}

bool AgentManager::canStartAgent() const
{
    return m_claudeAvailable && activeCount() < m_maxConcurrentAgents;
}

QString AgentManager::claudePath() const
{
    return m_claudePath;
}

void AgentManager::setMaxConcurrentAgents(int max)
{
    if (m_maxConcurrentAgents != max) {
        m_maxConcurrentAgents = max;
        Q_EMIT maxConcurrentAgentsChanged();
    }
}

int AgentManager::activeCount() const
{
    int count = 0;
    for (const auto *agent : m_agents) {
        if (agent->status() == AgentProcess::Running || agent->status() == AgentProcess::Starting)
            ++count;
    }
    return count;
}

void AgentManager::connectAgent(const QString &agentId, AgentProcess *agent)
{
    auto *model = m_outputModels.value(agentId);

    connect(agent, &AgentProcess::statusChanged, this, [this, agentId, agent]() {
        Q_EMIT agentStatusChanged(agentId, agent->status());
        Q_EMIT activeCountChanged();
    });

    connect(agent, &AgentProcess::activityChanged, this, [this, agentId, agent]() {
        Q_EMIT agentActivityChanged(agentId, agent->currentActivity());
    });

    connect(agent, &AgentProcess::costUpdated, this, [this, agentId](double cost) {
        Q_EMIT agentCostChanged(agentId, cost);
    });

    connect(agent, &AgentProcess::assistantText, this, [this, agentId, model](const QString &text) {
        if (model) model->appendText(text);
        Q_EMIT agentOutput(agentId, 0, text, QString());
    });

    connect(agent, &AgentProcess::thinkingText, this, [this, agentId, model](const QString &text) {
        if (model) model->appendThinking(text);
        Q_EMIT agentOutput(agentId, 1, text, QString());
    });

    connect(agent, &AgentProcess::toolUse, this, [this, agentId, model](const QString &toolName, const QJsonObject &input) {
        QString summary = toolName;
        if (input.contains(QStringLiteral("command")))
            summary += QStringLiteral(": ") + input[QStringLiteral("command")].toString();
        else if (input.contains(QStringLiteral("file_path")))
            summary += QStringLiteral(": ") + input[QStringLiteral("file_path")].toString();
        else if (input.contains(QStringLiteral("pattern")))
            summary += QStringLiteral(": ") + input[QStringLiteral("pattern")].toString();
        if (model) model->appendToolUse(toolName, summary);
        Q_EMIT agentOutput(agentId, 2, summary, toolName);
    });

    connect(agent, &AgentProcess::toolResult, this, [this, agentId, model](const QString &toolName, const QString &output) {
        if (model) model->appendToolResult(toolName, output);
        Q_EMIT agentOutput(agentId, 3, output, toolName);
    });

    connect(agent, &AgentProcess::errorOccurred, this, [this, agentId, model](const QString &error) {
        if (model) model->appendError(error);
        Q_EMIT agentOutput(agentId, 5, error, QString());
        Q_EMIT agentError(agentId, error);
    });

    connect(agent, &AgentProcess::processFinished, this, [this, agentId](int) {
        Q_EMIT agentFinished(agentId);
    });
}
