#include "agentmanager.h"
#include "agentprocess.h"
#include "agentoutputmodel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include <utility>

AgentManager::AgentManager(QObject *parent)
    : QObject(parent)
{
    // Load settings
    QSettings settings;
    m_maxConcurrentAgents = settings.value(QStringLiteral("maxConcurrentAgents"), 8).toInt();
    m_defaultModel = settings.value(QStringLiteral("defaultModel"), QStringLiteral("opus")).toString();
    m_showInTray = settings.value(QStringLiteral("showInTray"), true).toBool();
    m_notifyOnComplete = settings.value(QStringLiteral("notifyOnComplete"), true).toBool();
    m_permissionMode = settings.value(QStringLiteral("permissionMode"), QStringLiteral("bypass")).toString();
    m_extraFlags = settings.value(QStringLiteral("extraFlags")).toString();

    detectClaude();
    loadAgents();
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &AgentManager::saveAgents);
}

void AgentManager::detectClaude()
{
    m_claudePath.clear();
    m_claudeAvailable = false;

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

bool AgentManager::redetectClaude()
{
    bool wasAvailable = m_claudeAvailable;
    QString oldPath = m_claudePath;
    detectClaude();
    if (m_claudeAvailable != wasAvailable || m_claudePath != oldPath)
        Q_EMIT claudeAvailableChanged();
    return m_claudeAvailable;
}

static QString dataDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    // Agent transcripts and metadata can contain sensitive content; keep the
    // directory owner-only.
    QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return dir;
}

// Atomic, owner-only write. Returns false (and warns) on failure without
// clobbering the previous file contents.
static bool writeFileAtomic(const QString &path, const QByteArray &data)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("kductor: cannot write %s: %s", qUtf8Printable(path), qUtf8Printable(file.errorString()));
        return false;
    }
    file.write(data);
    if (!file.commit()) {
        qWarning("kductor: failed to commit %s: %s", qUtf8Printable(path), qUtf8Printable(file.errorString()));
        return false;
    }
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

static QString agentsFilePath()
{
    return dataDir() + QStringLiteral("/agents.json");
}

static QString outputFilePath(const QString &agentId)
{
    QString dir = dataDir() + QStringLiteral("/output");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/") + agentId + QStringLiteral(".json");
}

void AgentManager::saveAgentsMetadata()
{
    QJsonArray arr;
    for (const QString &agentId : m_agentOrder) {
        if (!m_agentToWorkspace.contains(agentId))
            continue;
        QJsonObject obj;
        obj[QStringLiteral("id")] = agentId;
        obj[QStringLiteral("workspaceId")] = m_agentToWorkspace.value(agentId);
        obj[QStringLiteral("name")] = m_agentNames.value(agentId);
        if (auto *agent = m_agents.value(agentId))
            obj[QStringLiteral("sessionId")] = agent->sessionId();
        arr.append(obj);
    }
    writeFileAtomic(agentsFilePath(), QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void AgentManager::saveAgentOutput(const QString &agentId)
{
    auto *model = m_outputModels.value(agentId);
    if (model && model->count() > 0)
        writeFileAtomic(outputFilePath(agentId),
                        QJsonDocument(model->toJson()).toJson(QJsonDocument::Compact));
}

void AgentManager::saveAgents()
{
    saveAgentsMetadata();
    for (const QString &agentId : m_agentOrder)
        saveAgentOutput(agentId);
}

void AgentManager::loadAgents()
{
    QFile file(agentsFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
    for (const auto &val : arr) {
        QJsonObject obj = val.toObject();
        QString agentId = obj[QStringLiteral("id")].toString();
        QString workspaceId = obj[QStringLiteral("workspaceId")].toString();
        QString name = obj[QStringLiteral("name")].toString();

        if (agentId.isEmpty() || workspaceId.isEmpty())
            continue;

        auto *agent = new AgentProcess(this);
        auto *model = new AgentOutputModel(this);

        // Restore the session id so a follow-up prompt continues the same
        // conversation the user sees in the transcript, not a fresh one.
        agent->setSessionId(obj[QStringLiteral("sessionId")].toString());
        agent->setPermissionMode(m_permissionMode);
        agent->setExtraFlags(m_extraFlags);

        // Restore output history
        QFile outFile(outputFilePath(agentId));
        if (outFile.open(QIODevice::ReadOnly)) {
            QJsonArray outputArr = QJsonDocument::fromJson(outFile.readAll()).array();
            model->loadFromJson(outputArr);
        }

        m_agents.insert(agentId, agent);
        m_outputModels.insert(agentId, model);
        m_agentToWorkspace.insert(agentId, workspaceId);
        m_agentNames.insert(agentId, name);
        m_agentOrder.append(agentId);
        connectAgent(agentId, agent);
    }
}

QString AgentManager::createAgent(const QString &workspaceId, const QString &name)
{
    auto *agent = new AgentProcess(this);
    auto *model = new AgentOutputModel(this);
    QString agentId = agent->agentId();

    agent->setPermissionMode(m_permissionMode);
    agent->setExtraFlags(m_extraFlags);

    m_agents.insert(agentId, agent);
    m_outputModels.insert(agentId, model);
    m_agentToWorkspace.insert(agentId, workspaceId);
    m_agentNames.insert(agentId, name);
    m_agentOrder.append(agentId);
    connectAgent(agentId, agent);

    saveAgentsMetadata();

    Q_EMIT agentSpawned(agentId, workspaceId);
    Q_EMIT activeCountChanged();
    return agentId;
}

AgentOutputModel *AgentManager::outputModel(const QString &agentId) const
{
    return m_outputModels.value(agentId, nullptr);
}

QString AgentManager::agentName(const QString &agentId) const
{
    return m_agentNames.value(agentId);
}

bool AgentManager::startAgent(const QString &agentId, const QString &workingDir,
                               const QString &prompt, const QString &model,
                               const QStringList &imagePaths)
{
    auto *agent = m_agents.value(agentId, nullptr);
    if (!agent || !canStartAgent())
        return false;
    bool launched = agent->start(workingDir, prompt, model, imagePaths);
    if (launched)
        Q_EMIT activeCountChanged();
    return launched;
}

bool AgentManager::sendPrompt(const QString &agentId, const QString &workingDir,
                               const QString &prompt, const QString &model,
                               const QStringList &imagePaths)
{
    auto *agent = m_agents.value(agentId, nullptr);
    if (!agent)
        return false;
    bool launched = agent->resume(workingDir, prompt, model, imagePaths);
    if (launched)
        Q_EMIT activeCountChanged();
    return launched;
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
    m_agentNames.remove(agentId);
    m_agentOrder.removeAll(agentId);
    m_pendingGitCommands.remove(agentId);
    QFile::remove(outputFilePath(agentId));

    saveAgentsMetadata();
    Q_EMIT activeCountChanged();
}

QStringList AgentManager::agentsForWorkspace(const QString &workspaceId) const
{
    // Iterate the stable order list so tabs keep a deterministic sequence across
    // restarts (a raw QHash iteration order is unspecified).
    QStringList result;
    for (const QString &agentId : m_agentOrder) {
        if (m_agentToWorkspace.value(agentId) == workspaceId)
            result.append(agentId);
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

QString AgentManager::agentSessionId(const QString &agentId) const
{
    auto *agent = m_agents.value(agentId, nullptr);
    return agent ? agent->sessionId() : QString();
}

double AgentManager::agentCost(const QString &agentId) const
{
    auto *agent = m_agents.value(agentId, nullptr);
    return agent ? agent->totalCost() : 0.0;
}

bool AgentManager::canStartAgent() const
{
    return m_claudeAvailable && activeCount() < m_maxConcurrentAgents;
}

int AgentManager::workspaceAgentStatus(const QString &workspaceId) const
{
    // Return 2 (Running) if any agent in this workspace is running/starting, else 0 (Idle)
    for (auto it = m_agentToWorkspace.cbegin(); it != m_agentToWorkspace.cend(); ++it) {
        if (it.value() == workspaceId) {
            auto *agent = m_agents.value(it.key());
            if (agent && (agent->status() == AgentProcess::Running || agent->status() == AgentProcess::Starting))
                return 2;
        }
    }
    return 0;
}

QString AgentManager::claudePath() const
{
    return m_claudePath;
}

void AgentManager::setMaxConcurrentAgents(int max)
{
    if (m_maxConcurrentAgents != max) {
        m_maxConcurrentAgents = max;
        QSettings().setValue(QStringLiteral("maxConcurrentAgents"), max);
        Q_EMIT maxConcurrentAgentsChanged();
    }
}

void AgentManager::setDefaultModel(const QString &model)
{
    if (m_defaultModel != model) {
        m_defaultModel = model;
        QSettings().setValue(QStringLiteral("defaultModel"), model);
        Q_EMIT defaultModelChanged();
    }
}

void AgentManager::setShowInTray(bool show)
{
    if (m_showInTray != show) {
        m_showInTray = show;
        QSettings().setValue(QStringLiteral("showInTray"), show);
        Q_EMIT showInTrayChanged();
    }
}

void AgentManager::setNotifyOnComplete(bool notify)
{
    if (m_notifyOnComplete != notify) {
        m_notifyOnComplete = notify;
        QSettings().setValue(QStringLiteral("notifyOnComplete"), notify);
        Q_EMIT notifyOnCompleteChanged();
    }
}

void AgentManager::setPermissionMode(const QString &mode)
{
    if (m_permissionMode != mode) {
        m_permissionMode = mode;
        QSettings().setValue(QStringLiteral("permissionMode"), mode);
        // Apply to existing agents so the next prompt uses the new mode.
        for (auto *agent : std::as_const(m_agents))
            agent->setPermissionMode(mode);
        Q_EMIT permissionModeChanged();
    }
}

void AgentManager::setExtraFlags(const QString &flags)
{
    if (m_extraFlags != flags) {
        m_extraFlags = flags;
        QSettings().setValue(QStringLiteral("extraFlags"), flags);
        for (auto *agent : std::as_const(m_agents))
            agent->setExtraFlags(flags);
        Q_EMIT extraFlagsChanged();
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
        QString detail;
        if (input.contains(QStringLiteral("command")))
            detail = input[QStringLiteral("command")].toString();
        else if (input.contains(QStringLiteral("file_path")))
            detail = input[QStringLiteral("file_path")].toString();
        else if (input.contains(QStringLiteral("pattern")))
            detail = input[QStringLiteral("pattern")].toString();

        // Strip long worktree paths — keep only the relative part
        if (!detail.isEmpty()) {
            int kductorIdx = detail.indexOf(QStringLiteral("/.kductor/worktrees/"));
            if (kductorIdx >= 0) {
                // Find the end of the worktree dir name (third / after .kductor)
                int afterWorktree = detail.indexOf(QLatin1Char('/'), kductorIdx + 20);
                if (afterWorktree >= 0) {
                    afterWorktree = detail.indexOf(QLatin1Char('/'), afterWorktree + 1);
                    if (afterWorktree >= 0)
                        detail = detail.mid(afterWorktree + 1);
                }
            }
        }

        QString summary = toolName + (detail.isEmpty() ? QString() : QStringLiteral(": ") + detail);
        if (model) model->appendToolUse(toolName, summary);
        Q_EMIT agentOutput(agentId, 2, summary, toolName);

        // Track git-related Bash commands for event detection
        if (toolName == QStringLiteral("Bash") && input.contains(QStringLiteral("command"))) {
            QString cmd = input[QStringLiteral("command")].toString();
            if (cmd.contains(QStringLiteral("git ")) || cmd.contains(QStringLiteral("gh ")))
                m_pendingGitCommands[agentId] = cmd;
        }
    });

    connect(agent, &AgentProcess::toolResult, this, [this, agentId, model](const QString &toolName, const QString &output) {
        if (model) model->appendToolResult(toolName, output);
        Q_EMIT agentOutput(agentId, 3, output, toolName);

        // Detect completed git operations from the agent's Bash tool calls
        QString pendingCmd = m_pendingGitCommands.take(agentId);
        if (!pendingCmd.isEmpty())
            detectGitEvent(agentId, pendingCmd);
    });

    connect(agent, &AgentProcess::errorOccurred, this, [this, agentId, model](const QString &error) {
        if (model) model->appendError(error);
        Q_EMIT agentOutput(agentId, 5, error, QString());
        Q_EMIT agentError(agentId, error);
    });

    connect(agent, &AgentProcess::contextUpdated, this, [this, agentId](int used, int window) {
        Q_EMIT agentContextUpdated(agentId, used, window);
    });

    connect(agent, &AgentProcess::processFinished, this, [this, agentId](int) {
        m_pendingGitCommands.remove(agentId);
        Q_EMIT agentFinished(agentId);
        // Persist the new session id and only this agent's transcript — not every
        // agent's full output — after each run.
        saveAgentsMetadata();
        saveAgentOutput(agentId);
    });
}

void AgentManager::detectGitEvent(const QString &agentId, const QString &command)
{
    QString wsId = m_agentToWorkspace.value(agentId);
    if (wsId.isEmpty())
        return;

    // Order matters — check more specific patterns first
    if (command.contains(QStringLiteral("gh pr create")))
        Q_EMIT gitEventDetected(wsId, QStringLiteral("pr-created"));
    else if (command.contains(QStringLiteral("gh pr merge")))
        Q_EMIT gitEventDetected(wsId, QStringLiteral("pr-merged"));
    else if (command.contains(QStringLiteral("git push")))
        Q_EMIT gitEventDetected(wsId, QStringLiteral("push"));
    else if (command.contains(QStringLiteral("git commit")))
        Q_EMIT gitEventDetected(wsId, QStringLiteral("commit"));
    else if (command.contains(QStringLiteral("git merge")))
        Q_EMIT gitEventDetected(wsId, QStringLiteral("merge"));
}
