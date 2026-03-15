#pragma once

#include <QObject>
#include <QString>
#include <QDir>

#include "workspace.h"

class GitManager;

class WorktreeManager : public QObject
{
    Q_OBJECT

public:
    explicit WorktreeManager(GitManager *gitManager, QObject *parent = nullptr);

    Q_INVOKABLE bool createWorkspace(const QString &name, const QString &repoPath,
                                     const QString &sourceBranch);
    Q_INVOKABLE bool removeWorkspace(const QString &workspaceId, const QString &repoPath,
                                     const QString &worktreeName);

    Q_INVOKABLE QString worktreeBasePath() const;

    Workspace lastCreatedWorkspace() const { return m_lastCreated; }

Q_SIGNALS:
    void workspaceCreated(const QString &id);
    void workspaceRemoved(const QString &id);
    void errorOccurred(const QString &message);

private:
    GitManager *m_gitManager;
    Workspace m_lastCreated;
    QString generateWorktreeName(const QString &workspaceName) const;
    QString generateBranchName(const QString &workspaceName) const;
};
