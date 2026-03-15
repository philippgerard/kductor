#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

class Workspace
{
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString repoPath MEMBER repoPath)
    Q_PROPERTY(QString worktreePath MEMBER worktreePath)
    Q_PROPERTY(QString branchName MEMBER branchName)
    Q_PROPERTY(QString sourceBranch MEMBER sourceBranch)
    Q_PROPERTY(int status MEMBER status)
    Q_PROPERTY(QDateTime createdAt MEMBER createdAt)

public:
    enum Status {
        Active = 0,
        Paused,
        Completed,
        Archived,
        Error
    };
    Q_ENUM(Status)

    QString id;
    QString name;
    QString repoPath;
    QString worktreePath;
    QString branchName;
    QString sourceBranch;
    int status = Active;
    QDateTime createdAt;
    QDateTime updatedAt;
    QStringList agentIds;

    static Workspace create(const QString &name, const QString &repoPath,
                            const QString &worktreePath, const QString &branchName,
                            const QString &sourceBranch);

    QJsonObject toJson() const;
    static Workspace fromJson(const QJsonObject &obj);
};

class WorkspaceStore : public QObject
{
    Q_OBJECT

public:
    explicit WorkspaceStore(QObject *parent = nullptr);

    QList<Workspace> loadAll() const;
    void saveAll(const QList<Workspace> &workspaces);
    void addWorkspace(const Workspace &ws);
    void updateWorkspace(const Workspace &ws);
    void removeWorkspace(const QString &id);

private:
    QString storagePath() const;
};
