#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <git2.h>

#include "core/worktreemanager.h"
#include "core/gitmanager.h"

// Same helper as gitmanagertest
static bool initRepoWithCommit(const QString &path)
{
    git_repository *repo = nullptr;
    if (git_repository_init(&repo, path.toUtf8().constData(), 0) < 0)
        return false;

    QFile f(path + QStringLiteral("/README"));
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write("init\n");
    f.close();

    git_index *index = nullptr;
    git_repository_index(&index, repo);
    git_index_add_bypath(index, "README");
    git_index_write(index);

    git_oid treeId;
    git_index_write_tree(&treeId, index);
    git_index_free(index);

    git_tree *tree = nullptr;
    git_tree_lookup(&tree, repo, &treeId);

    git_signature *sig = nullptr;
    git_signature_new(&sig, "Test", "test@test.com", 0, 0);

    git_oid commitId;
    git_commit_create_v(&commitId, repo, "HEAD", sig, sig,
                        nullptr, "Initial commit", tree, 0);

    git_reference *ref = nullptr;
    git_reference *newRef = nullptr;
    if (git_repository_head(&ref, repo) == 0) {
        git_reference_rename(&newRef, ref, "refs/heads/main", 1, "rename to main");
        if (newRef) git_reference_free(newRef);
        git_reference_free(ref);
    }
    git_repository_set_head(repo, "refs/heads/main");

    git_signature_free(sig);
    git_tree_free(tree);
    git_repository_free(repo);
    return true;
}

// Helper: add a remote to a git repo (no need for it to be reachable)
static bool addRemote(const QString &repoPath, const QString &url)
{
    git_repository *repo = nullptr;
    if (git_repository_open(&repo, repoPath.toUtf8().constData()) < 0)
        return false;

    git_remote *remote = nullptr;
    int err = git_remote_create(&remote, repo, "origin", url.toUtf8().constData());
    if (remote) git_remote_free(remote);
    git_repository_free(repo);
    return err == 0;
}

class WorktreeManagerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // --- Name generation tests ---

    void testGenerateWorktreeNameBasic()
    {
        GitManager git;
        WorktreeManager wt(&git);
        QString name = wt.generateWorktreeName(QStringLiteral("My Feature"), QStringLiteral("abc12345"));
        QCOMPARE(name, QStringLiteral("kductor-my-feature-abc12345"));
    }

    void testGenerateWorktreeNameSpecialChars()
    {
        GitManager git;
        WorktreeManager wt(&git);
        QString name = wt.generateWorktreeName(QStringLiteral("Fix Bug #123!"), QStringLiteral("deadbeef"));
        // Special chars replaced with hyphens
        QVERIFY(name.startsWith(QStringLiteral("kductor-fix-bug-")));
        QVERIFY(name.endsWith(QStringLiteral("-deadbeef")));
        // No special chars remain
        QVERIFY(!name.contains(QLatin1Char('#')));
        QVERIFY(!name.contains(QLatin1Char('!')));
    }

    void testGenerateWorktreeNameTruncation()
    {
        GitManager git;
        WorktreeManager wt(&git);
        // Very long name — should be truncated to 30 chars before suffix
        QString longName = QString(100, QLatin1Char('a'));
        QString name = wt.generateWorktreeName(longName, QStringLiteral("12345678"));
        // "kductor-" prefix (8) + truncated clean (30) + "-" (1) + suffix (8) = 47
        // The clean part before suffix should be at most 30 chars
        QVERIFY(name.startsWith(QStringLiteral("kductor-")));
        QVERIFY(name.endsWith(QStringLiteral("-12345678")));
    }

    void testGenerateWorktreeNameUnicode()
    {
        GitManager git;
        WorktreeManager wt(&git);
        // Non-ASCII chars should be replaced
        QString name = wt.generateWorktreeName(QStringLiteral("über-fix"), QStringLiteral("abcd1234"));
        QVERIFY(name.startsWith(QStringLiteral("kductor-")));
        // ü replaced → "ber-fix" or just "-ber-fix"
        QVERIFY(!name.contains(QStringLiteral("ü")));
    }

    void testGenerateBranchNameBasic()
    {
        GitManager git;
        WorktreeManager wt(&git);
        QString name = wt.generateBranchName(QStringLiteral("My Feature"), QStringLiteral("abc12345"));
        QCOMPARE(name, QStringLiteral("kductor/my-feature-abc12345"));
    }

    void testGenerateBranchNameTruncation()
    {
        GitManager git;
        WorktreeManager wt(&git);
        QString longName = QString(100, QLatin1Char('b'));
        QString name = wt.generateBranchName(longName, QStringLiteral("12345678"));
        // Clean part truncated to 40 chars
        QVERIFY(name.startsWith(QStringLiteral("kductor/")));
        QVERIFY(name.endsWith(QStringLiteral("-12345678")));
    }

    void testGenerateSuffixFormat()
    {
        GitManager git;
        WorktreeManager wt(&git);
        QString s1 = wt.generateSuffix();
        QString s2 = wt.generateSuffix();
        // 8-char hex-like UUID prefix
        QCOMPARE(s1.size(), 8);
        QCOMPARE(s2.size(), 8);
        QVERIFY(s1 != s2);
    }

    // --- Workspace creation tests ---

    void testCreateWorkspace()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));

        GitManager git;
        WorktreeManager wt(&git);
        QSignalSpy createSpy(&wt, &WorktreeManager::workspaceCreated);

        bool ok = wt.createWorkspace(QStringLiteral("test workspace"), repoDir.path(),
                                     QStringLiteral("main"));
        QVERIFY(ok);
        QCOMPARE(createSpy.count(), 1);

        auto ws = wt.lastCreatedWorkspace();
        QCOMPARE(ws.name, QStringLiteral("test workspace"));
        QCOMPARE(ws.repoPath, repoDir.path());
        QCOMPARE(ws.sourceBranch, QStringLiteral("main"));
        QVERIFY(!ws.branchName.isEmpty());
        QVERIFY(!ws.worktreePath.isEmpty());
        QVERIFY(QDir(ws.worktreePath).exists());
    }

    void testCreateWorkspaceInvalidRepo()
    {
        GitManager git;
        WorktreeManager wt(&git);
        QSignalSpy errorSpy(&wt, &WorktreeManager::errorOccurred);

        bool ok = wt.createWorkspace(QStringLiteral("fail"), QStringLiteral("/nonexistent"),
                                     QStringLiteral("main"));
        QVERIFY(!ok);
        QCOMPARE(errorSpy.count(), 1);
    }

    void testRemoveWorkspace()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));

        GitManager git;
        WorktreeManager wt(&git);
        wt.createWorkspace(QStringLiteral("to-remove"), repoDir.path(), QStringLiteral("main"));
        auto ws = wt.lastCreatedWorkspace();

        // Extract worktree name from the path
        QString wtName = QFileInfo(ws.worktreePath).fileName();

        QSignalSpy removeSpy(&wt, &WorktreeManager::workspaceRemoved);
        bool ok = wt.removeWorkspace(ws.id, repoDir.path(), wtName);
        QVERIFY(ok);
        QCOMPARE(removeSpy.count(), 1);
        QCOMPARE(removeSpy[0][0].toString(), ws.id);
    }

    void testWorktreeBasePath()
    {
        GitManager git;
        WorktreeManager wt(&git);
        QString base = wt.worktreeBasePath();
        QVERIFY(base.contains(QStringLiteral(".kductor/worktrees")));
    }

    // --- detectForge / remoteWebUrl / hasRemote tests ---

    void testHasRemoteNoRemote()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));

        GitManager git;
        WorktreeManager wt(&git);
        QVERIFY(!wt.hasRemote(repoDir.path()));
    }

    void testHasRemoteWithRemote()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));
        QVERIFY(addRemote(repoDir.path(), QStringLiteral("https://github.com/user/repo.git")));

        GitManager git;
        WorktreeManager wt(&git);
        QVERIFY(wt.hasRemote(repoDir.path()));
    }

    void testDetectForgeGithub()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));
        QVERIFY(addRemote(repoDir.path(), QStringLiteral("git@github.com:user/repo.git")));

        GitManager git;
        WorktreeManager wt(&git);
        QCOMPARE(wt.detectForge(repoDir.path()), QStringLiteral("github"));
    }

    void testDetectForgeGitea()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));
        QVERIFY(addRemote(repoDir.path(), QStringLiteral("https://gitea.example.com/user/repo.git")));

        GitManager git;
        WorktreeManager wt(&git);
        QCOMPARE(wt.detectForge(repoDir.path()), QStringLiteral("gitea"));
    }

    void testDetectForgeGitlab()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));
        QVERIFY(addRemote(repoDir.path(), QStringLiteral("https://gitlab.com/user/repo.git")));

        GitManager git;
        WorktreeManager wt(&git);
        QCOMPARE(wt.detectForge(repoDir.path()), QStringLiteral("gitlab"));
    }

    void testDetectForgeUnknown()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));
        // No remote → unknown

        GitManager git;
        WorktreeManager wt(&git);
        QCOMPARE(wt.detectForge(repoDir.path()), QStringLiteral("unknown"));
    }

    void testDetectForgeOther()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));
        QVERIFY(addRemote(repoDir.path(), QStringLiteral("https://custom-git.example.com/repo.git")));

        GitManager git;
        WorktreeManager wt(&git);
        QCOMPARE(wt.detectForge(repoDir.path()), QStringLiteral("other"));
    }

    void testRemoteWebUrlHttps()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));
        QVERIFY(addRemote(repoDir.path(), QStringLiteral("https://github.com/user/repo.git")));

        GitManager git;
        WorktreeManager wt(&git);
        QCOMPARE(wt.remoteWebUrl(repoDir.path()), QStringLiteral("https://github.com/user/repo"));
    }

    void testRemoteWebUrlGitAt()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));
        QVERIFY(addRemote(repoDir.path(), QStringLiteral("git@github.com:user/repo.git")));

        GitManager git;
        WorktreeManager wt(&git);
        QCOMPARE(wt.remoteWebUrl(repoDir.path()), QStringLiteral("https://github.com/user/repo"));
    }

    void testRemoteWebUrlSsh()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));
        QVERIFY(addRemote(repoDir.path(), QStringLiteral("ssh://git@gitlab.com:2222/user/repo.git")));

        GitManager git;
        WorktreeManager wt(&git);
        QString url = wt.remoteWebUrl(repoDir.path());
        QVERIFY(url.startsWith(QStringLiteral("https://")));
        QVERIFY(url.endsWith(QStringLiteral("/user/repo")));
        QVERIFY(!url.contains(QStringLiteral("2222")));
    }

    void testRemoteWebUrlNoGitSuffix()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));
        QVERIFY(addRemote(repoDir.path(), QStringLiteral("https://github.com/user/repo")));

        GitManager git;
        WorktreeManager wt(&git);
        QCOMPARE(wt.remoteWebUrl(repoDir.path()), QStringLiteral("https://github.com/user/repo"));
    }

    void testRemoteWebUrlNoRemote()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));

        GitManager git;
        WorktreeManager wt(&git);
        QVERIFY(wt.remoteWebUrl(repoDir.path()).isEmpty());
    }
};

QTEST_GUILESS_MAIN(WorktreeManagerTest)
#include "worktreemanagertest.moc"
