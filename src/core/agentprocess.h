#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QJsonObject>
#include <QUuid>

class AgentProcess : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString agentId READ agentId CONSTANT)
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY sessionIdChanged)
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(double totalCost READ totalCost NOTIFY costUpdated)
    Q_PROPERTY(QString currentActivity READ currentActivity NOTIFY activityChanged)

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
    int status() const { return m_status; }
    double totalCost() const { return m_totalCost; }
    QString currentActivity() const { return m_currentActivity; }

    Q_INVOKABLE void start(const QString &workingDir, const QString &prompt,
                           const QString &model = QStringLiteral("sonnet"));
    Q_INVOKABLE void resume(const QString &workingDir, const QString &prompt,
                            const QString &model = QStringLiteral("sonnet"));
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
    void errorOccurred(const QString &error);
    void processFinished(int exitCode);

private Q_SLOTS:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    QProcess *m_process;
    QString m_agentId;
    QString m_sessionId;
    int m_status = Idle;
    double m_totalCost = 0.0;
    QString m_currentActivity;
    QByteArray m_buffer;

    void setStatus(int status);
    void setActivity(const QString &activity);
    void parseLine(const QByteArray &line);
    QStringList buildArgs(const QString &prompt, const QString &model, bool isResume) const;
};
