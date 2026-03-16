#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/workspacemodel.h"
#include "core/worktreemanager.h"
#include "core/gitmanager.h"
#include "core/workspace.h"

class WorkspaceModelTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_tmpDir;

    Workspace makeWs(const QString &name, const QString &repoPath)
    {
        return Workspace::create(
            name, repoPath,
            QStringLiteral("/tmp/wt-") + name,
            QStringLiteral("kductor/") + name,
            QStringLiteral("main")
        );
    }

private Q_SLOTS:
    void init()
    {
        // Fresh temp dir per test to avoid cross-test pollution via WorkspaceStore persistence
        m_tmpDir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_tmpDir->isValid());
        qputenv("XDG_DATA_HOME", m_tmpDir->path().toUtf8());
    }

    void testInitiallyEmpty()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.count(), 0);
    }

    void testAddWorkspace()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);
        QSignalSpy countSpy(&model, &WorkspaceModel::countChanged);

        auto ws = makeWs(QStringLiteral("first"), QStringLiteral("/repo/a"));
        model.addWorkspace(ws);

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.count(), 1);
        QCOMPARE(countSpy.count(), 1);
        QCOMPARE(model.lastAddedId(), ws.id);
    }

    void testDataRoles()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        auto ws = makeWs(QStringLiteral("roles-test"), QStringLiteral("/repo/b"));
        ws.agentIds = {QStringLiteral("a1"), QStringLiteral("a2")};
        model.addWorkspace(ws);

        auto idx = model.index(0);
        QCOMPARE(model.data(idx, WorkspaceModel::IdRole).toString(), ws.id);
        QCOMPARE(model.data(idx, WorkspaceModel::NameRole).toString(), QStringLiteral("roles-test"));
        QCOMPARE(model.data(idx, WorkspaceModel::RepoPathRole).toString(), QStringLiteral("/repo/b"));
        QCOMPARE(model.data(idx, WorkspaceModel::BranchNameRole).toString(), QStringLiteral("kductor/roles-test"));
        QCOMPARE(model.data(idx, WorkspaceModel::SourceBranchRole).toString(), QStringLiteral("main"));
        QCOMPARE(model.data(idx, WorkspaceModel::StatusRole).toInt(), static_cast<int>(Workspace::Active));
        QCOMPARE(model.data(idx, WorkspaceModel::AgentCountRole).toInt(), 2);
    }

    void testRoleNames()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        auto roles = model.roleNames();
        QCOMPARE(roles[WorkspaceModel::IdRole], QByteArray("workspaceId"));
        QCOMPARE(roles[WorkspaceModel::NameRole], QByteArray("name"));
        QCOMPARE(roles[WorkspaceModel::RepoPathRole], QByteArray("repoPath"));
        QCOMPARE(roles[WorkspaceModel::StatusRole], QByteArray("status"));
    }

    void testGetValid()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        auto ws = makeWs(QStringLiteral("get-test"), QStringLiteral("/repo/c"));
        model.addWorkspace(ws);

        auto map = model.get(0);
        QCOMPARE(map[QStringLiteral("id")].toString(), ws.id);
        QCOMPARE(map[QStringLiteral("name")].toString(), QStringLiteral("get-test"));
    }

    void testGetInvalid()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        QVERIFY(model.get(-1).isEmpty());
        QVERIFY(model.get(0).isEmpty());
        QVERIFY(model.get(999).isEmpty());
    }

    void testGetById()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        auto ws1 = makeWs(QStringLiteral("findme"), QStringLiteral("/repo"));
        auto ws2 = makeWs(QStringLiteral("other"), QStringLiteral("/repo"));
        model.addWorkspace(ws1);
        model.addWorkspace(ws2);

        auto found = model.getById(ws1.id);
        QCOMPARE(found[QStringLiteral("name")].toString(), QStringLiteral("findme"));

        auto notFound = model.getById(QStringLiteral("no-such-id"));
        QVERIFY(notFound.isEmpty());
    }

    void testRemove()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        auto ws = makeWs(QStringLiteral("removeme"), QStringLiteral("/repo/rm"));
        model.addWorkspace(ws);
        QCOMPARE(model.count(), 1);

        QSignalSpy countSpy(&model, &WorkspaceModel::countChanged);
        model.remove(ws.id);

        QCOMPARE(model.count(), 0);
        QVERIFY(countSpy.count() > 0);
    }

    void testRemoveNonexistent()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        auto ws = makeWs(QStringLiteral("keep"), QStringLiteral("/repo"));
        model.addWorkspace(ws);

        model.remove(QStringLiteral("no-such-id"));
        QCOMPARE(model.count(), 1); // unchanged
    }

    void testRemoveAddsRepoToStandalone()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        auto ws = makeWs(QStringLiteral("last-ws"), QStringLiteral("/repo/standalone"));
        model.addWorkspace(ws);
        model.remove(ws.id);

        // The repo should now appear in uniqueRepoPaths as standalone
        auto repos = model.uniqueRepoPaths();
        QVERIFY(repos.contains(QStringLiteral("/repo/standalone")));
    }

    void testUpdateStatus()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        auto ws = makeWs(QStringLiteral("status-test"), QStringLiteral("/repo"));
        model.addWorkspace(ws);

        QSignalSpy dataSpy(&model, &WorkspaceModel::dataChanged);
        model.updateStatus(ws.id, Workspace::Completed);

        QCOMPARE(model.data(model.index(0), WorkspaceModel::StatusRole).toInt(),
                 static_cast<int>(Workspace::Completed));
        QCOMPARE(dataSpy.count(), 1);
    }

    void testRename()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        auto ws = makeWs(QStringLiteral("old-name"), QStringLiteral("/repo"));
        model.addWorkspace(ws);

        QSignalSpy dataSpy(&model, &WorkspaceModel::dataChanged);
        model.rename(ws.id, QStringLiteral("new-name"));

        QCOMPARE(model.data(model.index(0), WorkspaceModel::NameRole).toString(),
                 QStringLiteral("new-name"));
        QCOMPARE(dataSpy.count(), 1);
    }

    void testUniqueRepoPaths()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        model.addWorkspace(makeWs(QStringLiteral("a"), QStringLiteral("/repo/x")));
        model.addWorkspace(makeWs(QStringLiteral("b"), QStringLiteral("/repo/x"))); // duplicate
        model.addWorkspace(makeWs(QStringLiteral("c"), QStringLiteral("/repo/y")));

        auto repos = model.uniqueRepoPaths();
        QCOMPARE(repos.count(QStringLiteral("/repo/x")), 1); // deduplicated
        QVERIFY(repos.contains(QStringLiteral("/repo/y")));
    }

    void testAddRemoveRepo()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        QSignalSpy countSpy(&model, &WorkspaceModel::countChanged);

        model.addRepo(QStringLiteral("/standalone/repo"));
        QVERIFY(model.uniqueRepoPaths().contains(QStringLiteral("/standalone/repo")));
        QVERIFY(countSpy.count() > 0);

        // Adding same repo again is a no-op
        int prevCount = countSpy.count();
        model.addRepo(QStringLiteral("/standalone/repo"));
        QCOMPARE(countSpy.count(), prevCount);

        model.removeRepo(QStringLiteral("/standalone/repo"));
        QVERIFY(!model.uniqueRepoPaths().contains(QStringLiteral("/standalone/repo")));
    }

    void testWorkspacesForRepo()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        model.addWorkspace(makeWs(QStringLiteral("a"), QStringLiteral("/repo/A")));
        model.addWorkspace(makeWs(QStringLiteral("b"), QStringLiteral("/repo/A")));
        model.addWorkspace(makeWs(QStringLiteral("c"), QStringLiteral("/repo/B")));

        auto forA = model.workspacesForRepo(QStringLiteral("/repo/A"));
        QCOMPARE(forA.size(), 2);

        auto forB = model.workspacesForRepo(QStringLiteral("/repo/B"));
        QCOMPARE(forB.size(), 1);

        auto forNone = model.workspacesForRepo(QStringLiteral("/repo/none"));
        QVERIFY(forNone.isEmpty());
    }

    void testInvalidIndexData()
    {
        GitManager git;
        WorktreeManager wt(&git);
        WorkspaceModel model(&wt);

        model.addWorkspace(makeWs(QStringLiteral("x"), QStringLiteral("/r")));

        // Parent index should return 0
        QCOMPARE(model.rowCount(model.index(0)), 0);

        // Out of range
        QVERIFY(!model.data(model.index(5), WorkspaceModel::IdRole).isValid());
        QVERIFY(!model.data(model.index(-1), WorkspaceModel::IdRole).isValid());
    }
};

QTEST_GUILESS_MAIN(WorkspaceModelTest)
#include "workspacemodeltest.moc"
