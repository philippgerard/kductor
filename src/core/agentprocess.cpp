#include "agentprocess.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>

static QString findClaude()
{
    // Check common locations for the claude binary
    QStringList candidates = {
        QDir::homePath() + QStringLiteral("/.local/bin/claude"),
        QDir::homePath() + QStringLiteral("/.npm/bin/claude"),
        QStringLiteral("/usr/local/bin/claude"),
        QStringLiteral("/usr/bin/claude"),
    };
    for (const auto &path : candidates) {
        if (QFile::exists(path))
            return path;
    }
    // Fall back to PATH lookup
    QString found = QStandardPaths::findExecutable(QStringLiteral("claude"));
    if (!found.isEmpty())
        return found;
    // Last resort
    return QStringLiteral("claude");
}

AgentProcess::AgentProcess(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_agentId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &AgentProcess::onReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &AgentProcess::onReadyReadStderr);
    connect(m_process, &QProcess::finished, this, &AgentProcess::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &AgentProcess::onProcessError);

    // Ensure ~/.local/bin is in PATH for the subprocess
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = env.value(QStringLiteral("PATH"));
    QString localBin = QDir::homePath() + QStringLiteral("/.local/bin");
    if (!path.contains(localBin)) {
        env.insert(QStringLiteral("PATH"), localBin + QStringLiteral(":") + path);
    }
    m_process->setProcessEnvironment(env);
}

AgentProcess::~AgentProcess()
{
    stop();
}

QString AgentProcess::buildPromptWithImages(const QString &prompt, const QStringList &imagePaths)
{
    if (imagePaths.isEmpty())
        return prompt;

    QString combined;
    combined += QStringLiteral("[The user has attached screenshots. Use the Read tool to view each one before responding:\n");
    for (const auto &path : imagePaths) {
        combined += QStringLiteral("- ") + path + QStringLiteral("\n");
    }
    combined += QStringLiteral("]\n\n");
    if (!prompt.isEmpty())
        combined += prompt;
    return combined;
}

QStringList AgentProcess::buildArgs(const QString &prompt, const QString &model, bool isResume,
                                    const QStringList &imagePaths) const
{
    QStringList args;

    // Resume this agent's own session by id, not the most recent conversation in
    // the working directory (which may belong to a different agent in the same
    // worktree).
    if (isResume && !m_sessionId.isEmpty()) {
        args << QStringLiteral("--resume") << m_sessionId;
    }

    args << QStringLiteral("-p") << buildPromptWithImages(prompt, imagePaths);
    args << QStringLiteral("--output-format") << QStringLiteral("stream-json");
    args << QStringLiteral("--verbose");
    args << QStringLiteral("--model") << model;

    // Permission handling is an explicit, user-visible choice (see Settings).
    // "bypass" keeps the fully-autonomous behaviour; the other modes hand control
    // to the CLI's permission system.
    if (m_permissionMode == QStringLiteral("bypass") || m_permissionMode.isEmpty()) {
        args << QStringLiteral("--dangerously-skip-permissions");
    } else {
        args << QStringLiteral("--permission-mode") << m_permissionMode;
    }

    // User-supplied extra flags, parsed with shell-style quoting.
    if (!m_extraFlags.trimmed().isEmpty())
        args << QProcess::splitCommand(m_extraFlags);

    return args;
}

bool AgentProcess::start(const QString &workingDir, const QString &prompt,
                         const QString &model, const QStringList &imagePaths)
{
    if (m_status == Running || m_status == Starting
        || m_process->state() != QProcess::NotRunning)
        return false;

    setStatus(Starting);
    m_buffer.clear();
    m_stderrBuffer.clear();
    m_toolUseNames.clear();
    m_stopRequested = false;

    QString program = findClaude();
    QStringList args = buildArgs(prompt, model, false, imagePaths);

    m_process->setWorkingDirectory(workingDir);
    m_process->setProgram(program);
    m_process->setArguments(args);

    m_process->start();
    m_process->closeWriteChannel();
    return true;
}

bool AgentProcess::resume(const QString &workingDir, const QString &prompt,
                          const QString &model, const QStringList &imagePaths)
{
    if (m_status == Running || m_status == Starting
        || m_process->state() != QProcess::NotRunning)
        return false;
    if (m_sessionId.isEmpty()) {
        return start(workingDir, prompt, model, imagePaths);
    }

    setStatus(Starting);
    m_buffer.clear();
    m_stderrBuffer.clear();
    m_toolUseNames.clear();
    m_stopRequested = false;

    m_process->setWorkingDirectory(workingDir);
    m_process->setProgram(findClaude());
    m_process->setArguments(buildArgs(prompt, model, true, imagePaths));
    m_process->start();
    m_process->closeWriteChannel();
    return true;
}

void AgentProcess::onReadyReadStderr()
{
    // Accumulate stderr; emit it once on a failed exit rather than per chunk,
    // which would produce spurious "Agent Error" notifications mid-run.
    m_stderrBuffer.append(m_process->readAllStandardError());
}

void AgentProcess::stop()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_stopRequested = true;
        const qint64 pid = m_process->processId();
        m_process->terminate();
        // Escalate to kill after a grace period, without blocking the GUI thread.
        // Guard on the PID so a process that exited and was replaced by a new
        // run (stop-then-resend) is not killed by this stale timer.
        QTimer::singleShot(3000, this, [this, pid]() {
            if (m_process->state() != QProcess::NotRunning && m_process->processId() == pid)
                m_process->kill();
        });
    } else {
        setStatus(Idle);
    }
}

void AgentProcess::onReadyRead()
{
    m_buffer.append(m_process->readAllStandardOutput());

    while (true) {
        int newlineIdx = m_buffer.indexOf('\n');
        if (newlineIdx < 0)
            break;

        QByteArray line = m_buffer.left(newlineIdx).trimmed();
        m_buffer.remove(0, newlineIdx + 1);

        if (!line.isEmpty()) {
            parseLine(line);
        }
    }
}

void AgentProcess::parseLine(const QByteArray &line)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return;
    }

    QJsonObject obj = doc.object();
    QString type = obj[QStringLiteral("type")].toString();

    if (type == QStringLiteral("system")) {
        QString subtype = obj[QStringLiteral("subtype")].toString();
        if (subtype == QStringLiteral("init")) {
            m_sessionId = obj[QStringLiteral("session_id")].toString();
            Q_EMIT sessionIdChanged();
            setStatus(Running);
            setActivity(QStringLiteral("Initialized"));
            Q_EMIT initialized(obj);
        }
    } else if (type == QStringLiteral("assistant")) {
        QJsonObject message = obj[QStringLiteral("message")].toObject();

        // Detect context compaction
        QJsonValue ctxMgmt = message[QStringLiteral("context_management")];
        if (!ctxMgmt.isNull() && ctxMgmt.isObject()) {
            setActivity(QStringLiteral("Context compacted"));
            Q_EMIT assistantText(QStringLiteral("*Context was automatically compacted to fit the conversation window.*"));
        }

        QJsonArray content = message[QStringLiteral("content")].toArray();

        for (const auto &block : content) {
            QJsonObject b = block.toObject();
            QString blockType = b[QStringLiteral("type")].toString();

            if (blockType == QStringLiteral("text")) {
                QString text = b[QStringLiteral("text")].toString();
                setActivity(text.left(80));
                Q_EMIT assistantText(text);
            } else if (blockType == QStringLiteral("thinking")) {
                Q_EMIT thinkingText(b[QStringLiteral("thinking")].toString());
            } else if (blockType == QStringLiteral("tool_use")) {
                QString toolName = b[QStringLiteral("name")].toString();
                m_toolUseNames.insert(b[QStringLiteral("id")].toString(), toolName);
                setActivity(QStringLiteral("Using %1").arg(toolName));
                Q_EMIT toolUse(toolName, b[QStringLiteral("input")].toObject());
            }
        }
    } else if (type == QStringLiteral("user")) {
        // Tool results arrive in user-type messages; their content is a string or
        // an array of content blocks, and they reference the originating tool_use
        // by id rather than carrying the tool name.
        QJsonObject message = obj[QStringLiteral("message")].toObject();
        const QJsonArray content = message[QStringLiteral("content")].toArray();
        for (const auto &block : content) {
            QJsonObject b = block.toObject();
            if (b[QStringLiteral("type")].toString() == QStringLiteral("tool_result")) {
                QString toolUseId = b[QStringLiteral("tool_use_id")].toString();
                QString toolName = m_toolUseNames.value(toolUseId);
                Q_EMIT toolResult(toolName, extractToolResultText(b[QStringLiteral("content")]));
            }
        }
    } else if (type == QStringLiteral("result")) {
        m_totalCost = obj[QStringLiteral("total_cost_usd")].toDouble();
        Q_EMIT costUpdated(m_totalCost);

        // Extract context usage from modelUsage
        QJsonObject modelUsage = obj[QStringLiteral("modelUsage")].toObject();
        if (!modelUsage.isEmpty()) {
            // First model entry contains the info
            auto it = modelUsage.begin();
            if (it != modelUsage.end()) {
                QJsonObject mu = it.value().toObject();
                m_contextWindow = mu[QStringLiteral("contextWindow")].toInt();
                int input = mu[QStringLiteral("inputTokens")].toInt();
                int output = mu[QStringLiteral("outputTokens")].toInt();
                int cacheRead = mu[QStringLiteral("cacheReadInputTokens")].toInt();
                int cacheCreate = mu[QStringLiteral("cacheCreationInputTokens")].toInt();
                m_contextUsed = input + output + cacheRead + cacheCreate;
                Q_EMIT contextUpdated(m_contextUsed, m_contextWindow);
            }
        }

        setStatus(Completed);
        setActivity(QStringLiteral("Completed"));
        Q_EMIT resultReady(obj);
    }
}

QString AgentProcess::extractToolResultText(const QJsonValue &content)
{
    if (content.isString())
        return content.toString();
    if (content.isArray()) {
        QStringList parts;
        const QJsonArray arr = content.toArray();
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            if (o[QStringLiteral("type")].toString() == QStringLiteral("text"))
                parts << o[QStringLiteral("text")].toString();
        }
        return parts.join(QStringLiteral("\n"));
    }
    return {};
}

void AgentProcess::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // A user-requested stop is a normal stop, not a crash or error.
    if (m_stopRequested) {
        m_stopRequested = false;
        m_stderrBuffer.clear();
        setStatus(Idle);
        setActivity(QStringLiteral("Stopped"));
        Q_EMIT processFinished(exitCode);
        return;
    }

    if (exitStatus == QProcess::CrashExit) {
        setStatus(Error);
        setActivity(QStringLiteral("Crashed"));
        const QString detail = QString::fromUtf8(m_stderrBuffer.trimmed());
        Q_EMIT errorOccurred(detail.isEmpty() ? QStringLiteral("Agent process crashed") : detail);
    } else if (m_status != Completed) {
        setStatus(exitCode == 0 ? Completed : Error);
        setActivity(exitCode == 0 ? QStringLiteral("Completed") : QStringLiteral("Error"));
        if (exitCode != 0) {
            const QString detail = QString::fromUtf8(m_stderrBuffer.trimmed());
            Q_EMIT errorOccurred(detail.isEmpty()
                ? QStringLiteral("Agent exited with code %1").arg(exitCode) : detail);
        }
    }
    m_stderrBuffer.clear();
    Q_EMIT processFinished(exitCode);
}

void AgentProcess::onProcessError(QProcess::ProcessError error)
{
    // terminate()/kill() during a user-requested stop surfaces as Crashed here;
    // that is expected, so let onProcessFinished handle it as a normal stop.
    if (m_stopRequested)
        return;

    setStatus(Error);
    QString msg;
    switch (error) {
    case QProcess::FailedToStart:
        msg = QStringLiteral("Claude CLI not found. Is 'claude' installed and in PATH?");
        break;
    case QProcess::Timedout:
        msg = QStringLiteral("Agent process timed out");
        break;
    default:
        msg = QStringLiteral("Agent process error: %1").arg(error);
        break;
    }
    setActivity(QStringLiteral("Error"));
    Q_EMIT errorOccurred(msg);
}

void AgentProcess::setStatus(int status)
{
    if (m_status != status) {
        m_status = status;
        Q_EMIT statusChanged();
    }
}

void AgentProcess::setActivity(const QString &activity)
{
    if (m_currentActivity != activity) {
        m_currentActivity = activity;
        Q_EMIT activityChanged();
    }
}
