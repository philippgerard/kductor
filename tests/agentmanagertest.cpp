#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QStandardPaths>

#include "core/agentmanager.h"
#include "core/agentprocess.h"
#include "core/agentoutputmodel.h"

class AgentManagerTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_tmpDir;

private Q_SLOTS:
    void initTestCase()
    {
        m_tmpDir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_tmpDir->isValid());
        // Isolate all data/config from the real app
        qputenv("XDG_DATA_HOME", m_tmpDir->path().toUtf8());
        qputenv("XDG_CONFIG_HOME", (m_tmpDir->path() + QStringLiteral("/config")).toUtf8());
        QCoreApplication::setOrganizationName(QStringLiteral("kductor-test"));
        QCoreApplication::setApplicationName(QStringLiteral("kductor-test"));
    }

    void testCreateAgent()
    {
        AgentManager mgr;
        QSignalSpy spawnSpy(&mgr, &AgentManager::agentSpawned);
        QSignalSpy countSpy(&mgr, &AgentManager::activeCountChanged);

        QString agentId = mgr.createAgent(QStringLiteral("ws-1"), QStringLiteral("Agent Alpha"));

        QVERIFY(!agentId.isEmpty());
        QCOMPARE(spawnSpy.count(), 1);
        QCOMPARE(spawnSpy[0][0].toString(), agentId);
        QCOMPARE(spawnSpy[0][1].toString(), QStringLiteral("ws-1"));
        QCOMPARE(countSpy.count(), 1);

        QCOMPARE(mgr.agentName(agentId), QStringLiteral("Agent Alpha"));
        QVERIFY(mgr.outputModel(agentId) != nullptr);
    }

    void testAgentsForWorkspace()
    {
        AgentManager mgr;

        QString a1 = mgr.createAgent(QStringLiteral("ws-A"));
        QString a2 = mgr.createAgent(QStringLiteral("ws-A"));
        QString a3 = mgr.createAgent(QStringLiteral("ws-B"));

        auto wsA = mgr.agentsForWorkspace(QStringLiteral("ws-A"));
        QCOMPARE(wsA.size(), 2);
        QVERIFY(wsA.contains(a1));
        QVERIFY(wsA.contains(a2));

        auto wsB = mgr.agentsForWorkspace(QStringLiteral("ws-B"));
        QCOMPARE(wsB.size(), 1);
        QVERIFY(wsB.contains(a3));

        auto wsNone = mgr.agentsForWorkspace(QStringLiteral("ws-nonexistent"));
        QVERIFY(wsNone.isEmpty());
    }

    void testRemoveAgent()
    {
        AgentManager mgr;
        QString agentId = mgr.createAgent(QStringLiteral("ws-rm"));
        QVERIFY(mgr.outputModel(agentId) != nullptr);

        QSignalSpy countSpy(&mgr, &AgentManager::activeCountChanged);
        mgr.removeAgent(agentId);

        QVERIFY(mgr.outputModel(agentId) == nullptr);
        QVERIFY(mgr.agentName(agentId).isEmpty());
        QVERIFY(mgr.agentsForWorkspace(QStringLiteral("ws-rm")).isEmpty());
        QVERIFY(countSpy.count() > 0);
    }

    void testAgentStatusNonexistent()
    {
        AgentManager mgr;
        QCOMPARE(mgr.agentStatus(QStringLiteral("no-such-agent")), 0);
        QVERIFY(mgr.agentActivity(QStringLiteral("no-such-agent")).isEmpty());
    }

    void testAgentStatusIdle()
    {
        AgentManager mgr;
        QString agentId = mgr.createAgent(QStringLiteral("ws-status"));
        // Freshly created agent is Idle
        QCOMPARE(mgr.agentStatus(agentId), static_cast<int>(AgentProcess::Idle));
    }

    void testActiveCount()
    {
        AgentManager mgr;
        QCOMPARE(mgr.activeCount(), 0);

        mgr.createAgent(QStringLiteral("ws-1"));
        mgr.createAgent(QStringLiteral("ws-2"));
        // All idle — active count stays 0
        QCOMPARE(mgr.activeCount(), 0);
    }

    void testCanStartAgent()
    {
        AgentManager mgr;
        // canStartAgent depends on claudeAvailable — which may or may not be true
        // in the test environment. Test the logic boundary:

        // If claude is available, should be true when under max
        if (mgr.claudeAvailable()) {
            QVERIFY(mgr.canStartAgent());
        } else {
            QVERIFY(!mgr.canStartAgent());
        }
    }

    void testWorkspaceAgentStatusAllIdle()
    {
        AgentManager mgr;
        mgr.createAgent(QStringLiteral("ws-idle"));
        mgr.createAgent(QStringLiteral("ws-idle"));

        // All agents idle → workspace status should be 0
        QCOMPARE(mgr.workspaceAgentStatus(QStringLiteral("ws-idle")), 0);
    }

    void testWorkspaceAgentStatusNoAgents()
    {
        AgentManager mgr;
        QCOMPARE(mgr.workspaceAgentStatus(QStringLiteral("ws-empty")), 0);
    }

    void testSetMaxConcurrentAgents()
    {
        AgentManager mgr;
        QSignalSpy spy(&mgr, &AgentManager::maxConcurrentAgentsChanged);

        int original = mgr.maxConcurrentAgents();
        mgr.setMaxConcurrentAgents(4);
        QCOMPARE(mgr.maxConcurrentAgents(), 4);
        QCOMPARE(spy.count(), original == 4 ? 0 : 1);

        // No-op on same value
        spy.clear();
        mgr.setMaxConcurrentAgents(4);
        QCOMPARE(spy.count(), 0);
    }

    void testSetDefaultModel()
    {
        AgentManager mgr;
        QSignalSpy spy(&mgr, &AgentManager::defaultModelChanged);

        mgr.setDefaultModel(QStringLiteral("haiku"));
        QCOMPARE(mgr.defaultModel(), QStringLiteral("haiku"));
        QVERIFY(spy.count() > 0);

        spy.clear();
        mgr.setDefaultModel(QStringLiteral("haiku")); // same
        QCOMPARE(spy.count(), 0);
    }

    void testSetShowInTray()
    {
        AgentManager mgr;
        QSignalSpy spy(&mgr, &AgentManager::showInTrayChanged);

        bool current = mgr.showInTray();
        mgr.setShowInTray(!current);
        QCOMPARE(mgr.showInTray(), !current);
        QCOMPARE(spy.count(), 1);

        spy.clear();
        mgr.setShowInTray(!current); // same
        QCOMPARE(spy.count(), 0);
    }

    void testSetNotifyOnComplete()
    {
        AgentManager mgr;
        QSignalSpy spy(&mgr, &AgentManager::notifyOnCompleteChanged);

        bool current = mgr.notifyOnComplete();
        mgr.setNotifyOnComplete(!current);
        QCOMPARE(mgr.notifyOnComplete(), !current);
        QCOMPARE(spy.count(), 1);

        spy.clear();
        mgr.setNotifyOnComplete(!current); // same
        QCOMPARE(spy.count(), 0);
    }

    void testDetectGitEventCommit()
    {
        AgentManager mgr;
        QString agentId = mgr.createAgent(QStringLiteral("ws-git"));
        QSignalSpy gitSpy(&mgr, &AgentManager::gitEventDetected);

        mgr.detectGitEvent(agentId, QStringLiteral("git commit -m 'fix bug'"));
        QCOMPARE(gitSpy.count(), 1);
        QCOMPARE(gitSpy[0][0].toString(), QStringLiteral("ws-git"));
        QCOMPARE(gitSpy[0][1].toString(), QStringLiteral("commit"));
    }

    void testDetectGitEventPush()
    {
        AgentManager mgr;
        QString agentId = mgr.createAgent(QStringLiteral("ws-git"));
        QSignalSpy gitSpy(&mgr, &AgentManager::gitEventDetected);

        mgr.detectGitEvent(agentId, QStringLiteral("git push -u origin HEAD"));
        QCOMPARE(gitSpy.count(), 1);
        QCOMPARE(gitSpy[0][1].toString(), QStringLiteral("push"));
    }

    void testDetectGitEventPrCreate()
    {
        AgentManager mgr;
        QString agentId = mgr.createAgent(QStringLiteral("ws-git"));
        QSignalSpy gitSpy(&mgr, &AgentManager::gitEventDetected);

        mgr.detectGitEvent(agentId, QStringLiteral("gh pr create --title 'feat' --body 'desc'"));
        QCOMPARE(gitSpy.count(), 1);
        QCOMPARE(gitSpy[0][1].toString(), QStringLiteral("pr-created"));
    }

    void testDetectGitEventPrMerge()
    {
        AgentManager mgr;
        QString agentId = mgr.createAgent(QStringLiteral("ws-git"));
        QSignalSpy gitSpy(&mgr, &AgentManager::gitEventDetected);

        mgr.detectGitEvent(agentId, QStringLiteral("gh pr merge --merge --delete-branch"));
        QCOMPARE(gitSpy.count(), 1);
        QCOMPARE(gitSpy[0][1].toString(), QStringLiteral("pr-merged"));
    }

    void testDetectGitEventMerge()
    {
        AgentManager mgr;
        QString agentId = mgr.createAgent(QStringLiteral("ws-git"));
        QSignalSpy gitSpy(&mgr, &AgentManager::gitEventDetected);

        mgr.detectGitEvent(agentId, QStringLiteral("git merge feature-branch --no-edit"));
        QCOMPARE(gitSpy.count(), 1);
        QCOMPARE(gitSpy[0][1].toString(), QStringLiteral("merge"));
    }

    void testDetectGitEventNoAgent()
    {
        AgentManager mgr;
        QSignalSpy gitSpy(&mgr, &AgentManager::gitEventDetected);

        // Agent not in any workspace — should not emit
        mgr.detectGitEvent(QStringLiteral("nonexistent"), QStringLiteral("git push"));
        QCOMPARE(gitSpy.count(), 0);
    }

    void testDetectGitEventNonGitCommand()
    {
        AgentManager mgr;
        QString agentId = mgr.createAgent(QStringLiteral("ws-git"));
        QSignalSpy gitSpy(&mgr, &AgentManager::gitEventDetected);

        mgr.detectGitEvent(agentId, QStringLiteral("ls -la"));
        QCOMPARE(gitSpy.count(), 0);
    }

    void testDetectGitEventPriorityOrder()
    {
        // "gh pr create" contains "git " substring? No — "gh pr create" doesn't.
        // But "gh pr merge" takes priority over "git merge" if command has both?
        // Actually the patterns are checked in order: pr create > pr merge > push > commit > merge
        AgentManager mgr;
        QString agentId = mgr.createAgent(QStringLiteral("ws-git"));
        QSignalSpy gitSpy(&mgr, &AgentManager::gitEventDetected);

        // A command with both "git push" and "git commit" — push wins (checked first)
        mgr.detectGitEvent(agentId, QStringLiteral("git commit && git push"));
        QCOMPARE(gitSpy.count(), 1);
        QCOMPARE(gitSpy[0][1].toString(), QStringLiteral("push"));
    }

    void testClaudePath()
    {
        AgentManager mgr;
        // If claude is available, path should be non-empty
        if (mgr.claudeAvailable()) {
            QVERIFY(!mgr.claudePath().isEmpty());
        }
    }
};

QTEST_GUILESS_MAIN(AgentManagerTest)
#include "agentmanagertest.moc"
