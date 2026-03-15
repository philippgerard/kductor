#include "agentprocess.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcessEnvironment>
#include <QStandardPaths>

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

QStringList AgentProcess::buildArgs(const QString &prompt, const QString &model, bool isResume) const
{
    QStringList args;

    if (isResume) {
        args << QStringLiteral("--continue");
    }

    args << QStringLiteral("-p") << prompt;
    args << QStringLiteral("--output-format") << QStringLiteral("stream-json");
    args << QStringLiteral("--verbose");
    args << QStringLiteral("--model") << model;
    args << QStringLiteral("--dangerously-skip-permissions");

    return args;
}

void AgentProcess::start(const QString &workingDir, const QString &prompt, const QString &model)
{
    if (m_status == Running || m_status == Starting)
        return;

    setStatus(Starting);
    m_buffer.clear();

    QString program = findClaude();
    QStringList args = buildArgs(prompt, model, false);

    m_process->setWorkingDirectory(workingDir);
    m_process->setProgram(program);
    m_process->setArguments(args);

    m_process->start();
    m_process->closeWriteChannel();
}

void AgentProcess::resume(const QString &workingDir, const QString &prompt, const QString &model)
{
    if (m_status == Running || m_status == Starting)
        return;
    if (m_sessionId.isEmpty()) {
        start(workingDir, prompt, model);
        return;
    }

    setStatus(Starting);
    m_buffer.clear();

    m_process->setWorkingDirectory(workingDir);
    m_process->setProgram(findClaude());
    m_process->setArguments(buildArgs(prompt, model, true));
    m_process->start();
    m_process->closeWriteChannel();
}

void AgentProcess::onReadyReadStderr()
{
    QByteArray data = m_process->readAllStandardError();
    if (!data.isEmpty()) {
        Q_EMIT errorOccurred(QString::fromUtf8(data.trimmed()));
    }
}

void AgentProcess::stop()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }
    setStatus(Idle);
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
                setActivity(QStringLiteral("Using %1").arg(toolName));
                Q_EMIT toolUse(toolName, b[QStringLiteral("input")].toObject());
            } else if (blockType == QStringLiteral("tool_result")) {
                Q_EMIT toolResult(
                    b[QStringLiteral("name")].toString(),
                    b[QStringLiteral("content")].toString()
                );
            }
        }
    } else if (type == QStringLiteral("result")) {
        m_totalCost = obj[QStringLiteral("total_cost_usd")].toDouble();
        Q_EMIT costUpdated(m_totalCost);
        setStatus(Completed);
        setActivity(QStringLiteral("Completed"));
        Q_EMIT resultReady(obj);
    }
}

void AgentProcess::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::CrashExit) {
        setStatus(Error);
        setActivity(QStringLiteral("Crashed"));
        Q_EMIT errorOccurred(QStringLiteral("Agent process crashed"));
    } else if (m_status != Completed) {
        setStatus(exitCode == 0 ? Completed : Error);
        setActivity(exitCode == 0 ? QStringLiteral("Completed") : QStringLiteral("Error"));
    }
    Q_EMIT processFinished(exitCode);
}

void AgentProcess::onProcessError(QProcess::ProcessError error)
{
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
