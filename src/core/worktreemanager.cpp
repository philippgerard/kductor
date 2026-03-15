#include "worktreemanager.h"
#include "gitmanager.h"

#include <QDir>
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

QString WorktreeManager::generateWorktreeName(const QString &workspaceName) const
{
    QString clean = workspaceName.toLower();
    clean.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    clean.truncate(30);
    QString shortId = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    return QStringLiteral("kductor-%1-%2").arg(clean, shortId);
}

QString WorktreeManager::generateBranchName(const QString &workspaceName) const
{
    QString clean = workspaceName.toLower();
    clean.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    clean.truncate(40);
    return QStringLiteral("kductor/%1").arg(clean);
}

bool WorktreeManager::createWorkspace(const QString &name, const QString &repoPath,
                                      const QString &sourceBranch)
{
    if (!m_gitManager->openRepository(repoPath)) {
        Q_EMIT errorOccurred(QStringLiteral("Failed to open repository: %1").arg(repoPath));
        return false;
    }

    QString wtName = generateWorktreeName(name);
    QString branchName = generateBranchName(name);

    QFileInfo repoInfo(repoPath);
    QString wtPath = worktreeBasePath() + QStringLiteral("/")
                     + repoInfo.fileName() + QStringLiteral("/") + wtName;

    QDir().mkpath(QFileInfo(wtPath).absolutePath());

    if (!m_gitManager->createWorktree(wtName, wtPath, branchName)) {
        Q_EMIT errorOccurred(QStringLiteral("Failed to create worktree"));
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
