#include "worktreemanager.h"
#include "gitmanager.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
        // Only handle the start failure here; crashes and other errors also emit
        // finished(), which already reports the failure — avoid a double signal.
        if (err != QProcess::FailedToStart)
            return;
        Q_EMIT operationFailed(operation, proc->errorString());
        proc->deleteLater();
    });

    proc->start();
    proc->closeWriteChannel();
}

// --- Phase 4 operations ---

void WorktreeManager::commitAll(const QString &worktreePath, const QString &message)
{
    QString msg = message.trimmed();
    if (msg.isEmpty())
        msg = QStringLiteral("Commit uncommitted changes");

    // The message is passed as a positional argument ($1) so it can never be
    // interpreted as shell syntax, no matter what the user types.
    QString script = QStringLiteral("git add -A && git commit -m \"$1\"");
    runAsync(QStringLiteral("commit"), worktreePath,
             QStringLiteral("bash"),
             {QStringLiteral("-c"), script, QStringLiteral("kductor"), msg});
}

void WorktreeManager::pushBranch(const QString &worktreePath)
{
    // Check if remote exists first
    QProcess check;
    check.setWorkingDirectory(worktreePath);
    check.start(QStringLiteral("git"), {QStringLiteral("remote")});
    check.waitForFinished(3000);
    QString remotes = QString::fromUtf8(check.readAllStandardOutput()).trimmed();
    if (remotes.isEmpty()) {
        Q_EMIT operationFailed(QStringLiteral("push"),
            QStringLiteral("No remote configured. Add one with: git remote add origin <url>"));
        return;
    }
    runAsync(QStringLiteral("push"), worktreePath,
             QStringLiteral("git"),
             {QStringLiteral("push"), QStringLiteral("-u"), QStringLiteral("origin"), QStringLiteral("HEAD")});
}

void WorktreeManager::createPullRequest(const QString &worktreePath, const QString &title, const QString &body)
{
    Q_UNUSED(title)
    Q_UNUSED(body)

    // Use Claude Haiku to generate a good PR title and body from the diff,
    // then create the PR with gh
    QString claudePath = QStandardPaths::findExecutable(QStringLiteral("claude"));
    if (claudePath.isEmpty()) {
        QStringList candidates = {
            QDir::homePath() + QStringLiteral("/.local/bin/claude"),
            QStringLiteral("/usr/local/bin/claude"),
        };
        for (const auto &c : candidates) {
            if (QFile::exists(c)) { claudePath = c; break; }
        }
    }

    QString script = QStringLiteral(
        "set -e\n"
        "BASE=$(git merge-base HEAD $(git rev-parse --abbrev-ref HEAD@{upstream} 2>/dev/null || echo main) 2>/dev/null || git rev-list --max-parents=0 HEAD)\n"
        "CONTEXT=\"Commits:\\n$(git log $BASE..HEAD --format='- %s' --reverse)\\n\\nDiff stat:\\n$(git diff $BASE --stat)\\n\\nSample changes:\\n$(git diff $BASE --no-color | head -200)\"\n"
        "\n"
        "PROMPT=\"Based on these git changes, write a pull request title and markdown body. "
        "Format your response exactly as:\\nTITLE: <concise title under 72 chars>\\nBODY:\\n<markdown body with a summary section and key changes>\"\n"
        "\n"
        "RESULT=$('%1' -p \"$PROMPT\\n\\n$CONTEXT\" --model haiku --output-format text 2>/dev/null)\n"
        "\n"
        "PR_TITLE=$(echo \"$RESULT\" | grep '^TITLE:' | head -1 | sed 's/^TITLE: *//')\n"
        "PR_BODY=$(echo \"$RESULT\" | sed -n '/^BODY:/,$ p' | tail -n +2)\n"
        "\n"
        "# Fallback if Claude didn't respond properly\n"
        "if [ -z \"$PR_TITLE\" ]; then\n"
        "  PR_TITLE=$(git log -1 --format='%s')\n"
        "fi\n"
        "if [ -z \"$PR_BODY\" ]; then\n"
        "  PR_BODY=$(git log $BASE..HEAD --format='- %s' --reverse)\n"
        "fi\n"
        "\n"
        "gh pr create --title \"$PR_TITLE\" --body \"$PR_BODY\"\n"
    ).arg(claudePath);

    runAsync(QStringLiteral("pr"), worktreePath,
             QStringLiteral("bash"), {QStringLiteral("-c"), script});
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
    // Merge the workspace branch into the source branch in the main repo.
    // Branch names are passed as positional arguments ($1 = source, $2 = branch)
    // so they can never be interpreted as shell syntax (a malicious ref name in
    // an untrusted repo would otherwise be command injection). The merge exit
    // code is captured and propagated so a conflict surfaces as a failed
    // operation instead of a false "Merged" success.
    // Uncommitted changes in the main repo are stashed and restored around it.
    QString script = QStringLiteral(
        "git stash --quiet 2>/dev/null || true\n"
        "if ! git checkout \"$1\" 2>&1; then\n"
        "  git stash pop --quiet 2>/dev/null || true\n"
        "  exit 1\n"
        "fi\n"
        "git merge \"$2\" --no-edit 2>&1\n"
        "RC=$?\n"
        "git stash pop --quiet 2>/dev/null || true\n"
        "exit $RC\n"
    );
    runAsync(QStringLiteral("merge"), repoPath,
             QStringLiteral("bash"),
             {QStringLiteral("-c"), script, QStringLiteral("kductor"),
              sourceBranch, branchName});
}

void WorktreeManager::archiveWorkspace(const QString &worktreePath, const QString &repoPath,
                                       const QString &deleteBranch)
{
    // Guard the recursive delete: refuse anything that is not inside our managed
    // worktree base directory. An empty or relative path would otherwise resolve
    // to (and wipe) the current working directory.
    const QString cleaned = QDir::cleanPath(QFileInfo(worktreePath).absoluteFilePath());
    const QString base = QDir::cleanPath(worktreeBasePath());
    if (worktreePath.isEmpty() || !cleaned.startsWith(base + QLatin1Char('/'))) {
        Q_EMIT operationFailed(QStringLiteral("archive"),
            QStringLiteral("Refusing to archive: '%1' is not inside the managed worktree directory.")
                .arg(worktreePath));
        return;
    }

    QDir(cleaned).removeRecursively();

    if (!deleteBranch.isEmpty()) {
        // Prune worktree then delete branch ($1, passed as a positional argument).
        QString script = QStringLiteral("git worktree prune && git branch -D \"$1\" 2>&1");
        runAsync(QStringLiteral("archive"), repoPath,
                 QStringLiteral("bash"),
                 {QStringLiteral("-c"), script, QStringLiteral("kductor"), deleteBranch});
    } else {
        runAsync(QStringLiteral("archive"), repoPath,
                 QStringLiteral("git"),
                 {QStringLiteral("worktree"), QStringLiteral("prune")});
    }
}

void WorktreeManager::checkPrStatus(const QString &worktreePath)
{
    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(worktreePath);
    proc->setProgram(QStringLiteral("gh"));
    proc->setArguments({QStringLiteral("pr"), QStringLiteral("view"),
                        QStringLiteral("--json"), QStringLiteral("url,number,state,mergeable,statusCheckRollup")});

    // If gh is missing or fails to launch, report "no PR" once and clean up the
    // process instead of leaking it every poll interval.
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError err) {
        if (err != QProcess::FailedToStart)
            return; // finished() handles crashes and other errors
        Q_EMIT prStatusChecked(QString(), 0, QStringLiteral("none"), QString(), QString());
        proc->deleteLater();
    });

    connect(proc, &QProcess::finished, this, [this, proc](int exitCode, QProcess::ExitStatus) {
        QString output = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();

        if (exitCode != 0 || output.isEmpty()) {
            Q_EMIT prStatusChecked(QString(), 0, QStringLiteral("none"), QString(), QString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
        QJsonObject obj = doc.object();

        QString url = obj[QStringLiteral("url")].toString();
        int number = obj[QStringLiteral("number")].toInt();
        QString state = obj[QStringLiteral("state")].toString(); // OPEN, MERGED, CLOSED
        QString mergeable = obj[QStringLiteral("mergeable")].toString(); // MERGEABLE, CONFLICTING, UNKNOWN

        // Aggregate status checks
        QString checks = QStringLiteral("none");
        QJsonArray rollup = obj[QStringLiteral("statusCheckRollup")].toArray();
        if (!rollup.isEmpty()) {
            bool allSuccess = true;
            bool anyFailure = false;
            for (const auto &c : rollup) {
                QString conclusion = c.toObject()[QStringLiteral("conclusion")].toString();
                QString status = c.toObject()[QStringLiteral("status")].toString();
                if (conclusion == QStringLiteral("FAILURE") || conclusion == QStringLiteral("ERROR"))
                    anyFailure = true;
                if (conclusion != QStringLiteral("SUCCESS") && status != QStringLiteral("COMPLETED"))
                    allSuccess = false;
            }
            if (anyFailure) checks = QStringLiteral("failure");
            else if (allSuccess) checks = QStringLiteral("success");
            else checks = QStringLiteral("pending");
        }

        Q_EMIT prStatusChecked(url, number, state, mergeable, checks);
    });

    proc->start();
    proc->closeWriteChannel();
}

void WorktreeManager::pullSource(const QString &repoPath, const QString &sourceBranch)
{
    // Branch name passed as a positional argument ($1) to avoid shell injection.
    QString script = QStringLiteral(
        "git fetch origin \"$1\" 2>&1 && git checkout \"$1\" 2>&1 && git pull --ff-only 2>&1"
    );
    runAsync(QStringLiteral("pull"), repoPath,
             QStringLiteral("bash"),
             {QStringLiteral("-c"), script, QStringLiteral("kductor"), sourceBranch});
}

bool WorktreeManager::hasRemote(const QString &worktreePath) const
{
    QProcess proc;
    proc.setWorkingDirectory(worktreePath);
    proc.start(QStringLiteral("git"), {QStringLiteral("remote")});
    if (!proc.waitForFinished(3000))
        return false;
    return !QString::fromUtf8(proc.readAllStandardOutput()).trimmed().isEmpty();
}

static QString forgeFromUrl(const QString &rawUrl)
{
    const QString url = rawUrl.toLower();
    if (url.isEmpty())
        return QStringLiteral("unknown");
    if (url.contains(QStringLiteral("github.com")))
        return QStringLiteral("github");
    if (url.contains(QStringLiteral("gitea")) || url.contains(QStringLiteral("forgejo")))
        return QStringLiteral("gitea");
    if (url.contains(QStringLiteral("gitlab")))
        return QStringLiteral("gitlab");
    // Has a remote but not a known forge — assume self-hosted.
    return QStringLiteral("other");
}

static QString webUrlFromRemote(QString url)
{
    if (url.isEmpty())
        return {};

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

    if (url.endsWith(QStringLiteral(".git")))
        url.chop(4);

    return url;
}

static QString readRemoteUrl(const QString &worktreePath)
{
    QProcess proc;
    proc.setWorkingDirectory(worktreePath);
    proc.start(QStringLiteral("git"), {QStringLiteral("remote"), QStringLiteral("get-url"), QStringLiteral("origin")});
    if (!proc.waitForFinished(5000))
        return {};
    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
}

QString WorktreeManager::detectForge(const QString &worktreePath) const
{
    return forgeFromUrl(readRemoteUrl(worktreePath));
}

QString WorktreeManager::remoteWebUrl(const QString &worktreePath) const
{
    return webUrlFromRemote(readRemoteUrl(worktreePath));
}

QVariantMap WorktreeManager::remoteInfo(const QString &worktreePath) const
{
    // One git invocation instead of three on the workspace-open path.
    const QString url = readRemoteUrl(worktreePath);
    return {
        {QStringLiteral("hasRemote"), !url.isEmpty()},
        {QStringLiteral("forge"), forgeFromUrl(url)},
        {QStringLiteral("webUrl"), webUrlFromRemote(url)},
    };
}
