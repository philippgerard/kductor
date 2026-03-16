#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include <git2.h>

#include "core/gitmanager.h"

// Helper: create a git repo with an initial commit so branches/worktrees work
static bool initRepoWithCommit(const QString &path)
{
    git_repository *repo = nullptr;
    if (git_repository_init(&repo, path.toUtf8().constData(), 0) < 0)
        return false;

    // Create an empty file and add it to the index
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

    // Rename default branch to "main" for consistency
    git_reference *ref = nullptr;
    git_reference *newRef = nullptr;
    if (git_repository_head(&ref, repo) == 0) {
        git_reference_rename(&newRef, ref, "refs/heads/main", 1, "rename to main");
        if (newRef)
            git_reference_free(newRef);
        git_reference_free(ref);
    }

    // Point HEAD at main
    git_repository_set_head(repo, "refs/heads/main");

    git_signature_free(sig);
    git_tree_free(tree);
    git_repository_free(repo);
    return true;
}

class GitManagerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testOpenInvalidPath()
    {
        GitManager mgr;
        QVERIFY(!mgr.openRepository(QStringLiteral("/nonexistent/path")));
        QVERIFY(!mgr.isOpen());
    }

    void testOpenValidRepo()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(initRepoWithCommit(tmp.path()));

        GitManager mgr;
        QVERIFY(mgr.openRepository(tmp.path()));
        QVERIFY(mgr.isOpen());
        QVERIFY(!mgr.repositoryPath().isEmpty());
    }

    void testRepositoryName()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(initRepoWithCommit(tmp.path()));

        GitManager mgr;
        mgr.openRepository(tmp.path());
        // The name should be the directory basename
        QVERIFY(!mgr.repositoryName().isEmpty());
    }

    void testCloseRepository()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(initRepoWithCommit(tmp.path()));

        GitManager mgr;
        mgr.openRepository(tmp.path());
        QVERIFY(mgr.isOpen());

        mgr.closeRepository();
        QVERIFY(!mgr.isOpen());
        QVERIFY(mgr.repositoryPath().isEmpty());
    }

    void testCurrentBranch()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(initRepoWithCommit(tmp.path()));

        GitManager mgr;
        mgr.openRepository(tmp.path());
        QCOMPARE(mgr.currentBranch(), QStringLiteral("main"));
    }

    void testListBranches()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(initRepoWithCommit(tmp.path()));

        GitManager mgr;
        mgr.openRepository(tmp.path());

        auto branches = mgr.listBranches();
        QVERIFY(branches.contains(QStringLiteral("main")));
    }

    void testListBranchesNotOpen()
    {
        GitManager mgr;
        QVERIFY(mgr.listBranches().isEmpty());
        QVERIFY(mgr.currentBranch().isEmpty());
    }

    void testCreateAndListWorktree()
    {
        QTemporaryDir repoDir;
        QTemporaryDir wtDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(wtDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));

        GitManager mgr;
        mgr.openRepository(repoDir.path());

        QString wtPath = wtDir.path() + QStringLiteral("/my-worktree");
        QVERIFY(mgr.createWorktree(QStringLiteral("test-wt"), wtPath,
                                    QStringLiteral("feature-x"), QStringLiteral("main")));

        auto worktrees = mgr.listWorktrees();
        QVERIFY(worktrees.contains(QStringLiteral("test-wt")));

        // The new branch should exist
        auto branches = mgr.listBranches();
        QVERIFY(branches.contains(QStringLiteral("feature-x")));
    }

    void testRemoveWorktree()
    {
        QTemporaryDir repoDir;
        QTemporaryDir wtDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(wtDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));

        GitManager mgr;
        mgr.openRepository(repoDir.path());

        QString wtPath = wtDir.path() + QStringLiteral("/wt-remove");
        mgr.createWorktree(QStringLiteral("wt-rm"), wtPath,
                           QStringLiteral("remove-branch"), QStringLiteral("main"));

        QVERIFY(mgr.listWorktrees().contains(QStringLiteral("wt-rm")));

        QVERIFY(mgr.removeWorktree(QStringLiteral("wt-rm")));
        QVERIFY(!mgr.listWorktrees().contains(QStringLiteral("wt-rm")));
    }

    void testRemoveNonexistentWorktree()
    {
        QTemporaryDir repoDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));

        GitManager mgr;
        mgr.openRepository(repoDir.path());
        QVERIFY(!mgr.removeWorktree(QStringLiteral("does-not-exist")));
    }

    void testGetStatusCleanRepo()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(initRepoWithCommit(tmp.path()));

        GitManager mgr;
        mgr.openRepository(tmp.path());

        auto status = mgr.getStatus();
        QVERIFY(status.isEmpty());
    }

    void testGetStatusWithChanges()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(initRepoWithCommit(tmp.path()));

        // Create an untracked file
        QFile f(tmp.path() + QStringLiteral("/newfile.txt"));
        f.open(QIODevice::WriteOnly);
        f.write("new content");
        f.close();

        GitManager mgr;
        mgr.openRepository(tmp.path());

        auto status = mgr.getStatus();
        QVERIFY(!status.isEmpty());

        bool found = false;
        for (const auto &entry : status) {
            auto map = entry.toMap();
            if (map[QStringLiteral("path")].toString() == QStringLiteral("newfile.txt")) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testHasUncommittedChanges()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(initRepoWithCommit(tmp.path()));

        GitManager mgr;
        mgr.openRepository(tmp.path());

        QVERIFY(!mgr.hasUncommittedChanges(tmp.path()));

        // Modify a tracked file
        QFile f(tmp.path() + QStringLiteral("/README"));
        f.open(QIODevice::WriteOnly);
        f.write("modified\n");
        f.close();

        QVERIFY(mgr.hasUncommittedChanges(tmp.path()));
    }

    void testGetDiffCleanRepo()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(initRepoWithCommit(tmp.path()));

        GitManager mgr;
        mgr.openRepository(tmp.path());

        auto diff = mgr.getDiff();
        QVERIFY(diff.isEmpty());
    }

    void testGetDetailedDiffInWorktree()
    {
        QTemporaryDir repoDir;
        QTemporaryDir wtDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(wtDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));

        GitManager mgr;
        mgr.openRepository(repoDir.path());

        QString wtPath = wtDir.path() + QStringLiteral("/diff-wt");
        mgr.createWorktree(QStringLiteral("diff-wt"), wtPath,
                           QStringLiteral("diff-branch"), QStringLiteral("main"));

        // Modify a file in the worktree
        QFile f(wtPath + QStringLiteral("/README"));
        f.open(QIODevice::WriteOnly);
        f.write("changed in worktree\n");
        f.close();

        // Mode 0: all changes (source vs workdir)
        auto diff = mgr.getDetailedDiff(wtPath, QStringLiteral("main"), 0);
        QVERIFY(!diff.isEmpty());

        auto firstFile = diff[0].toMap();
        QVERIFY(firstFile.contains(QStringLiteral("hunks")));
        QCOMPARE(firstFile[QStringLiteral("statusLabel")].toString(), QStringLiteral("Modified"));

        // Mode 2: pending (HEAD vs workdir) — same change since no commits on branch
        auto pending = mgr.getDetailedDiff(wtPath, QStringLiteral("main"), 2);
        QVERIFY(!pending.isEmpty());

        // Mode 1: committed (source vs HEAD) — nothing committed yet
        auto committed = mgr.getDetailedDiff(wtPath, QStringLiteral("main"), 1);
        QVERIFY(committed.isEmpty());
    }

    void testGetDetailedDiffInvalidPath()
    {
        GitManager mgr;
        auto diff = mgr.getDetailedDiff(QStringLiteral("/nonexistent"), QStringLiteral("main"), 0);
        QVERIFY(diff.isEmpty());
    }

    void testErrorSignal()
    {
        GitManager mgr;
        QSignalSpy errorSpy(&mgr, &GitManager::errorOccurred);

        mgr.openRepository(QStringLiteral("/definitely/not/a/repo"));

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(!errorSpy[0][0].toString().isEmpty());
    }

    void testMultipleWorktrees()
    {
        QTemporaryDir repoDir;
        QTemporaryDir wtDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(wtDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));

        GitManager mgr;
        mgr.openRepository(repoDir.path());

        QVERIFY(mgr.createWorktree(QStringLiteral("wt-a"), wtDir.path() + QStringLiteral("/a"),
                                    QStringLiteral("branch-a"), QStringLiteral("main")));
        QVERIFY(mgr.createWorktree(QStringLiteral("wt-b"), wtDir.path() + QStringLiteral("/b"),
                                    QStringLiteral("branch-b"), QStringLiteral("main")));

        auto worktrees = mgr.listWorktrees();
        QCOMPARE(worktrees.size(), 2);
        QVERIFY(worktrees.contains(QStringLiteral("wt-a")));
        QVERIFY(worktrees.contains(QStringLiteral("wt-b")));
    }

    void testDuplicateWorktreeFails()
    {
        QTemporaryDir repoDir;
        QTemporaryDir wtDir;
        QVERIFY(repoDir.isValid());
        QVERIFY(wtDir.isValid());
        QVERIFY(initRepoWithCommit(repoDir.path()));

        GitManager mgr;
        mgr.openRepository(repoDir.path());

        QVERIFY(mgr.createWorktree(QStringLiteral("dup-wt"), wtDir.path() + QStringLiteral("/dup"),
                                    QStringLiteral("dup-branch"), QStringLiteral("main")));
        // Same branch name should fail
        QVERIFY(!mgr.createWorktree(QStringLiteral("dup-wt2"), wtDir.path() + QStringLiteral("/dup2"),
                                     QStringLiteral("dup-branch"), QStringLiteral("main")));
    }
};

QTEST_GUILESS_MAIN(GitManagerTest)
#include "gitmanagertest.moc"
