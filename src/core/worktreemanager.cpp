#include "worktreemanager.h"
#include "gitmanager.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
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

// --- Async command runner ---

void WorktreeManager::runAsync(const QString &operation, const QString &workDir,
                               const QString &program, const QStringList &args)
{
    Q_EMIT operationStarted(operation);

    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(workDir);
    proc->setProgram(program);
    proc->setArguments(args);

    connect(proc, &QProcess::finished, this, [this, proc, operation](int exitCode, QProcess::ExitStatus status) {
        QString stdOut = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        QString stdErr = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        proc->deleteLater();

        if (status == QProcess::CrashExit || exitCode != 0) {
            QString error = stdErr.isEmpty() ? stdOut : stdErr;
            if (error.isEmpty())
                error = QStringLiteral("Process exited with code %1").arg(exitCode);
            Q_EMIT operationFailed(operation, error);
        } else {
            Q_EMIT operationSucceeded(operation, stdOut);
        }
    });

    connect(proc, &QProcess::errorOccurred, this, [this, proc, operation](QProcess::ProcessError err) {
        Q_UNUSED(err)
        Q_EMIT operationFailed(operation, proc->errorString());
        proc->deleteLater();
    });

    proc->start();
}

// --- Phase 4 operations ---

void WorktreeManager::pushBranch(const QString &worktreePath)
{
    runAsync(QStringLiteral("push"), worktreePath,
             QStringLiteral("git"),
             {QStringLiteral("push"), QStringLiteral("-u"), QStringLiteral("origin"), QStringLiteral("HEAD")});
}

void WorktreeManager::createPullRequest(const QString &worktreePath, const QString &title, const QString &body)
{
    QStringList args = {QStringLiteral("pr"), QStringLiteral("create")};
    if (!title.isEmpty())
        args << QStringLiteral("--title") << title;
    if (!body.isEmpty())
        args << QStringLiteral("--body") << body;
    else
        args << QStringLiteral("--fill-verbose");
    runAsync(QStringLiteral("pr"), worktreePath, QStringLiteral("gh"), args);
}

void WorktreeManager::mergePullRequest(const QString &worktreePath)
{
    runAsync(QStringLiteral("merge-pr"), worktreePath,
             QStringLiteral("gh"),
             {QStringLiteral("pr"), QStringLiteral("merge"),
              QStringLiteral("--merge"), QStringLiteral("--delete-branch")});
}

void WorktreeManager::mergeToSource(const QString &repoPath, const QString &branchName, const QString &sourceBranch)
{
    // Merge locally: stash any changes, checkout source, merge, restore
    QString script = QStringLiteral(
        "set -e\n"
        "git stash --quiet 2>/dev/null || true\n"
        "git checkout '%1'\n"
        "git merge '%2' --no-edit\n"
        "git stash pop --quiet 2>/dev/null || true\n"
    ).arg(sourceBranch, branchName);
    runAsync(QStringLiteral("merge"), repoPath,
             QStringLiteral("bash"),
             {QStringLiteral("-c"), script});
}

void WorktreeManager::archiveWorkspace(const QString &worktreePath, const QString &repoPath)
{
    // Remove worktree directory and prune
    QDir(worktreePath).removeRecursively();

    runAsync(QStringLiteral("archive"), repoPath,
             QStringLiteral("git"),
             {QStringLiteral("worktree"), QStringLiteral("prune")});
}
