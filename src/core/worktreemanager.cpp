#include "worktreemanager.h"
#include "gitmanager.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>

WorktreeManager::WorktreeManager(GitManager *gitManager, QObject *parent)
    : QObject(parent)
    , m_gitManager(gitManager)
{
}

QString WorktreeManager::worktreeBasePath() const
{
    return QDir::homePath() + QStringLiteral("/.kductor/worktrees");
}

QString WorktreeManager::generateSuffix() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

QString WorktreeManager::generateWorktreeName(const QString &workspaceName, const QString &suffix) const
{
    QString clean = workspaceName.toLower();
    clean.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    clean.truncate(30);
    return QStringLiteral("kductor-%1-%2").arg(clean, suffix);
}

QString WorktreeManager::generateBranchName(const QString &workspaceName, const QString &suffix) const
{
    QString clean = workspaceName.toLower();
    clean.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    clean.truncate(40);
    return QStringLiteral("kductor/%1-%2").arg(clean, suffix);
}

bool WorktreeManager::createWorkspace(const QString &name, const QString &repoPath,
                                      const QString &sourceBranch)
{
    if (!m_gitManager->openRepository(repoPath)) {
        Q_EMIT errorOccurred(QStringLiteral("Failed to open repository: %1").arg(repoPath));
        return false;
    }

    QString suffix = generateSuffix();
    QString wtName = generateWorktreeName(name, suffix);
    QString branchName = generateBranchName(name, suffix);

    QFileInfo repoInfo(repoPath);
    QString wtPath = worktreeBasePath() + QStringLiteral("/")
                     + repoInfo.fileName() + QStringLiteral("/") + wtName;

    QDir().mkpath(QFileInfo(wtPath).absolutePath());

    if (!m_gitManager->createWorktree(wtName, wtPath, branchName, sourceBranch)) {
        Q_EMIT errorOccurred(m_gitManager->lastError());
        return false;
    }

    m_lastCreated = Workspace::create(name, repoPath, wtPath, branchName, sourceBranch);
    Q_EMIT workspaceCreated(m_lastCreated.id);
    return true;
}

bool WorktreeManager::removeWorkspace(const QString &workspaceId, const QString &repoPath,
                                      const QString &worktreeName)
{
    if (!m_gitManager->openRepository(repoPath)) {
        Q_EMIT errorOccurred(QStringLiteral("Failed to open repository"));
        return false;
    }

    if (!m_gitManager->removeWorktree(worktreeName)) {
        Q_EMIT errorOccurred(QStringLiteral("Failed to remove worktree"));
        return false;
    }

    Q_EMIT workspaceRemoved(workspaceId);
    return true;
}
