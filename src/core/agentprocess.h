#pragma once

#include <QObject>
#include <QHash>
#include <QProcess>
#include <QString>
#include <QJsonObject>
#include <QJsonValue>
#include <QUuid>

class AgentProcess : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString agentId READ agentId CONSTANT)
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY sessionIdChanged)
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(double totalCost READ totalCost NOTIFY costUpdated)
    Q_PROPERTY(QString currentActivity READ currentActivity NOTIFY activityChanged)
    Q_PROPERTY(int contextUsed READ contextUsed NOTIFY contextUpdated)
    Q_PROPERTY(int contextWindow READ contextWindow NOTIFY contextUpdated)

public:
    enum Status {
        Idle = 0,
        Starting,
        Running,
        Completed,
        Error
    };
    Q_ENUM(Status)

    explicit AgentProcess(QObject *parent = nullptr);
    ~AgentProcess() override;

    QString agentId() const { return m_agentId; }
    QString sessionId() const { return m_sessionId; }
    void setSessionId(const QString &id) { m_sessionId = id; }
    // Permission mode passed to the claude CLI: "bypass" (--dangerously-skip-permissions),
    // or one of "default"/"acceptEdits"/"plan" (--permission-mode <mode>).
    void setPermissionMode(const QString &mode) { m_permissionMode = mode; }
    // Extra CLI flags appended verbatim (parsed with shell-style quoting).
    void setExtraFlags(const QString &flags) { m_extraFlags = flags; }
    int status() const { return m_status; }
    double totalCost() const { return m_totalCost; }
    QString currentActivity() const { return m_currentActivity; }
    int contextUsed() const { return m_contextUsed; }
    int contextWindow() const { return m_contextWindow; }

    // Return true if the process was actually launched; false if the agent was
    // busy (so the caller knows the prompt was not delivered).
    Q_INVOKABLE bool start(const QString &workingDir, const QString &prompt,
                           const QString &model = QStringLiteral("sonnet"),
                           const QStringList &imagePaths = {});
    Q_INVOKABLE bool resume(const QString &workingDir, const QString &prompt,
                            const QString &model = QStringLiteral("sonnet"),
                            const QStringList &imagePaths = {});
    Q_INVOKABLE void stop();

Q_SIGNALS:
    void sessionIdChanged();
    void statusChanged();
    void costUpdated(double cost);
    void activityChanged();

    void initialized(const QJsonObject &systemInfo);
    void assistantText(const QString &text);
    void thinkingText(const QString &thought);
    void toolUse(const QString &toolName, const QJsonObject &input);
    void toolResult(const QString &toolName, const QString &output);
    void resultReady(const QJsonObject &result);
    void contextUpdated(int used, int window);
    void errorOccurred(const QString &error);
    void processFinished(int exitCode);

private Q_SLOTS:
    void onReadyRead();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

    friend class AgentProcessTest;

private:
    QProcess *m_process;
    QString m_agentId;
    QString m_sessionId;
    int m_status = Idle;
    double m_totalCost = 0.0;
    QString m_currentActivity;
    int m_contextUsed = 0;
    int m_contextWindow = 0;
    QByteArray m_buffer;
    QByteArray m_stderrBuffer;
    bool m_stopRequested = false;
    QString m_permissionMode = QStringLiteral("bypass");
    QString m_extraFlags;
    QHash<QString, QString> m_toolUseNames; // tool_use_id -> tool name

    void setStatus(int status);
    void setActivity(const QString &activity);
    void parseLine(const QByteArray &line);
    QStringList buildArgs(const QString &prompt, const QString &model, bool isResume,
                          const QStringList &imagePaths = {}) const;
    static QString buildPromptWithImages(const QString &prompt, const QStringList &imagePaths);
    static QString extractToolResultText(const QJsonValue &content);
};
