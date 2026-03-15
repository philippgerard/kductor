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

    // Phase 4: PR & merge workflow
    Q_INVOKABLE void pushBranch(const QString &worktreePath);
    Q_INVOKABLE void createPullRequest(const QString &worktreePath, const QString &title, const QString &body);
    Q_INVOKABLE void mergePullRequest(const QString &worktreePath);
    Q_INVOKABLE void mergeToSource(const QString &repoPath, const QString &branchName, const QString &sourceBranch);
    Q_INVOKABLE void archiveWorkspace(const QString &worktreePath, const QString &repoPath,
                                      const QString &deleteBranch = QString());
    Q_INVOKABLE bool hasRemote(const QString &worktreePath) const;
    Q_INVOKABLE void pullSource(const QString &repoPath, const QString &sourceBranch);
    Q_INVOKABLE void checkPrStatus(const QString &worktreePath);
    Q_INVOKABLE QString detectForge(const QString &worktreePath) const; // "github", "gitea", or "unknown"
    Q_INVOKABLE QString remoteWebUrl(const QString &worktreePath) const;

    Workspace lastCreatedWorkspace() const { return m_lastCreated; }

Q_SIGNALS:
    void workspaceCreated(const QString &id);
    void workspaceRemoved(const QString &id);
    void errorOccurred(const QString &message);

    void operationStarted(const QString &operation);
    void operationSucceeded(const QString &operation, const QString &result);
    void operationFailed(const QString &operation, const QString &error);
    void prStatusChecked(const QString &url, int number, const QString &state,
                         const QString &mergeable, const QString &checks);

private:
    GitManager *m_gitManager;
    Workspace m_lastCreated;
    QString generateSuffix() const;
    QString generateWorktreeName(const QString &workspaceName, const QString &suffix) const;
    QString generateBranchName(const QString &workspaceName, const QString &suffix) const;
    void runAsync(const QString &operation, const QString &workDir,
                  const QString &program, const QStringList &args);
};
