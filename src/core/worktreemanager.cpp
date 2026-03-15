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

        // Combine output for display
        QString combined = stdOut;
        if (!stdErr.isEmpty()) {
            if (!combined.isEmpty()) combined += QStringLiteral("\n");
            combined += stdErr;
        }

        if (status == QProcess::CrashExit || exitCode != 0) {
            if (combined.isEmpty())
                combined = QStringLiteral("Process exited with code %1").arg(exitCode);
            Q_EMIT operationFailed(operation, combined);
        } else {
            Q_EMIT operationSucceeded(operation, combined);
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
    // First: find and auto-commit any uncommitted changes in the worktree
    // Then: in the main repo, checkout source branch and merge
    // We need the worktree path to commit changes there first
    // Find worktree path from branch name
    QString findWt = QStringLiteral(
        "WT_PATH=$(git worktree list --porcelain | grep -B1 'branch refs/heads/%1' | head -1 | sed 's/worktree //')\n"
        "if [ -n \"$WT_PATH\" ] && [ -d \"$WT_PATH\" ]; then\n"
        "  cd \"$WT_PATH\"\n"
        "  if ! git diff --quiet HEAD 2>/dev/null || ! git diff --cached --quiet HEAD 2>/dev/null; then\n"
        "    git add -A\n"
        "    git commit -m 'Auto-commit uncommitted changes before merge' --quiet\n"
        "    echo 'Committed pending changes in worktree'\n"
        "  fi\n"
        "  cd '%2'\n"
        "fi\n"
        "git stash --quiet 2>/dev/null || true\n"
        "git checkout '%3' 2>&1\n"
        "git merge '%1' --no-edit 2>&1\n"
        "git stash pop --quiet 2>/dev/null || true\n"
    ).arg(branchName, repoPath, sourceBranch);
    runAsync(QStringLiteral("merge"), repoPath,
             QStringLiteral("bash"),
             {QStringLiteral("-c"), findWt});
}

void WorktreeManager::archiveWorkspace(const QString &worktreePath, const QString &repoPath)
{
    QDir(worktreePath).removeRecursively();

    runAsync(QStringLiteral("archive"), repoPath,
             QStringLiteral("git"),
             {QStringLiteral("worktree"), QStringLiteral("prune")});
}

QString WorktreeManager::detectForge(const QString &worktreePath) const
{
    QProcess proc;
    proc.setWorkingDirectory(worktreePath);
    proc.start(QStringLiteral("git"), {QStringLiteral("remote"), QStringLiteral("get-url"), QStringLiteral("origin")});
    if (!proc.waitForFinished(5000))
        return QStringLiteral("unknown");

    QString url = QString::fromUtf8(proc.readAllStandardOutput()).trimmed().toLower();
    if (url.contains(QStringLiteral("github.com")))
        return QStringLiteral("github");
    if (url.contains(QStringLiteral("gitea")) || url.contains(QStringLiteral("forgejo")))
        return QStringLiteral("gitea");
    if (url.contains(QStringLiteral("gitlab")))
        return QStringLiteral("gitlab");
    // If it has a remote but not a known forge, assume self-hosted
    if (!url.isEmpty())
        return QStringLiteral("other");
    return QStringLiteral("unknown");
}

QString WorktreeManager::remoteWebUrl(const QString &worktreePath) const
{
    QProcess proc;
    proc.setWorkingDirectory(worktreePath);
    proc.start(QStringLiteral("git"), {QStringLiteral("remote"), QStringLiteral("get-url"), QStringLiteral("origin")});
    if (!proc.waitForFinished(5000))
        return {};

    QString url = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();

    // Convert SSH URL to HTTPS: ssh://git@host:port/user/repo.git -> https://host/user/repo
    if (url.startsWith(QStringLiteral("ssh://"))) {
        url.remove(0, 6); // remove ssh://
        url.replace(QRegularExpression(QStringLiteral("^[^@]*@")), QString()); // remove user@
        url.replace(QRegularExpression(QStringLiteral(":\\d+")), QString()); // remove :port
        url = QStringLiteral("https://") + url;
    }
    // Convert git@host:user/repo.git -> https://host/user/repo
    else if (url.startsWith(QStringLiteral("git@"))) {
        url.remove(0, 4);
        url.replace(QLatin1Char(':'), QLatin1Char('/'));
        url = QStringLiteral("https://") + url;
    }

    // Remove .git suffix
    if (url.endsWith(QStringLiteral(".git")))
        url.chop(4);

    return url;
}
