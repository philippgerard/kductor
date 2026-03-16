#include <QTest>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "core/workspace.h"

class WorkspaceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCreate()
    {
        auto ws = Workspace::create(
            QStringLiteral("my workspace"),
            QStringLiteral("/home/user/repo"),
            QStringLiteral("/tmp/worktrees/ws-abc123"),
            QStringLiteral("kductor/my-workspace-abc123"),
            QStringLiteral("main")
        );

        QCOMPARE(ws.name, QStringLiteral("my workspace"));
        QCOMPARE(ws.repoPath, QStringLiteral("/home/user/repo"));
        QCOMPARE(ws.worktreePath, QStringLiteral("/tmp/worktrees/ws-abc123"));
        QCOMPARE(ws.branchName, QStringLiteral("kductor/my-workspace-abc123"));
        QCOMPARE(ws.sourceBranch, QStringLiteral("main"));
        QCOMPARE(ws.status, static_cast<int>(Workspace::Active));
        QVERIFY(!ws.id.isEmpty());
        QVERIFY(ws.createdAt.isValid());
        QCOMPARE(ws.createdAt, ws.updatedAt);
    }

    void testUniqueIds()
    {
        auto ws1 = Workspace::create(QStringLiteral("a"), QStringLiteral("/r"),
                                     QStringLiteral("/w1"), QStringLiteral("b1"), QStringLiteral("main"));
        auto ws2 = Workspace::create(QStringLiteral("b"), QStringLiteral("/r"),
                                     QStringLiteral("/w2"), QStringLiteral("b2"), QStringLiteral("main"));
        QVERIFY(ws1.id != ws2.id);
    }

    void testJsonRoundTrip()
    {
        auto ws = Workspace::create(
            QStringLiteral("test ws"),
            QStringLiteral("/repo/path"),
            QStringLiteral("/worktree/path"),
            QStringLiteral("feature-branch"),
            QStringLiteral("develop")
        );
        ws.status = Workspace::Paused;
        ws.agentIds = {QStringLiteral("agent-1"), QStringLiteral("agent-2")};

        QJsonObject json = ws.toJson();

        QCOMPARE(json[QStringLiteral("name")].toString(), QStringLiteral("test ws"));
        QCOMPARE(json[QStringLiteral("repoPath")].toString(), QStringLiteral("/repo/path"));
        QCOMPARE(json[QStringLiteral("status")].toInt(), static_cast<int>(Workspace::Paused));
        QCOMPARE(json[QStringLiteral("agentIds")].toArray().size(), 2);

        auto restored = Workspace::fromJson(json);

        QCOMPARE(restored.id, ws.id);
        QCOMPARE(restored.name, ws.name);
        QCOMPARE(restored.repoPath, ws.repoPath);
        QCOMPARE(restored.worktreePath, ws.worktreePath);
        QCOMPARE(restored.branchName, ws.branchName);
        QCOMPARE(restored.sourceBranch, ws.sourceBranch);
        QCOMPARE(restored.status, ws.status);
        // ISODate serialization truncates milliseconds, so compare to the second
        QCOMPARE(restored.createdAt.toSecsSinceEpoch(), ws.createdAt.toSecsSinceEpoch());
        QCOMPARE(restored.updatedAt.toSecsSinceEpoch(), ws.updatedAt.toSecsSinceEpoch());
        QCOMPARE(restored.agentIds, ws.agentIds);
    }

    void testFromJsonEmptyObject()
    {
        QJsonObject empty;
        auto ws = Workspace::fromJson(empty);

        QVERIFY(ws.id.isEmpty());
        QVERIFY(ws.name.isEmpty());
        QCOMPARE(ws.status, 0);
        QVERIFY(ws.agentIds.isEmpty());
    }

    void testFromJsonNoAgents()
    {
        QJsonObject obj;
        obj[QStringLiteral("id")] = QStringLiteral("test-id");
        obj[QStringLiteral("name")] = QStringLiteral("no agents");
        obj[QStringLiteral("status")] = 0;

        auto ws = Workspace::fromJson(obj);
        QCOMPARE(ws.id, QStringLiteral("test-id"));
        QVERIFY(ws.agentIds.isEmpty());
    }

    void testStoreRoundTrip()
    {
        // Use a temporary dir to avoid touching real app data
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Set XDG_DATA_HOME so WorkspaceStore writes to our temp dir
        qputenv("XDG_DATA_HOME", tmpDir.path().toUtf8());

        WorkspaceStore store;

        // Initially empty
        QVERIFY(store.loadAll().isEmpty());

        // Add workspaces
        auto ws1 = Workspace::create(QStringLiteral("ws1"), QStringLiteral("/repo"),
                                     QStringLiteral("/wt1"), QStringLiteral("b1"), QStringLiteral("main"));
        auto ws2 = Workspace::create(QStringLiteral("ws2"), QStringLiteral("/repo"),
                                     QStringLiteral("/wt2"), QStringLiteral("b2"), QStringLiteral("main"));
        store.addWorkspace(ws1);
        store.addWorkspace(ws2);

        // Reload from disk
        WorkspaceStore store2;
        auto loaded = store2.loadAll();
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded[0].id, ws1.id);
        QCOMPARE(loaded[1].id, ws2.id);

        // Update
        ws1.status = Workspace::Completed;
        store2.updateWorkspace(ws1);

        auto loaded2 = store2.loadAll();
        QCOMPARE(loaded2[0].status, static_cast<int>(Workspace::Completed));

        // Remove
        store2.removeWorkspace(ws1.id);
        auto loaded3 = store2.loadAll();
        QCOMPARE(loaded3.size(), 1);
        QCOMPARE(loaded3[0].id, ws2.id);
    }

    void testStoreUpdateNonexistent()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        qputenv("XDG_DATA_HOME", tmpDir.path().toUtf8());

        WorkspaceStore store;

        auto ws = Workspace::create(QStringLiteral("ghost"), QStringLiteral("/r"),
                                    QStringLiteral("/w"), QStringLiteral("b"), QStringLiteral("main"));
        // Updating a workspace that doesn't exist should not crash
        store.updateWorkspace(ws);
        QVERIFY(store.loadAll().isEmpty());
    }

    void testAllStatusValues()
    {
        for (int s = Workspace::Active; s <= Workspace::Error; ++s) {
            auto ws = Workspace::create(QStringLiteral("s%1").arg(s), QStringLiteral("/r"),
                                        QStringLiteral("/w"), QStringLiteral("b"), QStringLiteral("main"));
            ws.status = s;
            auto json = ws.toJson();
            auto restored = Workspace::fromJson(json);
            QCOMPARE(restored.status, s);
        }
    }
};

QTEST_GUILESS_MAIN(WorkspaceTest)
#include "workspacetest.moc"
