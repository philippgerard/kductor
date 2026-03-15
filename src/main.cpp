#include <QAction>
#include <QApplication>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QWindow>

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedContext>
#include <KLocalizedString>
#include <KCrash>
#include <KIconTheme>
#include <KNotification>
#include <KStatusNotifierItem>

#include "kductor-version.h"
#include "core/gitmanager.h"
#include "core/worktreemanager.h"
#include "core/workspacemodel.h"
#include "core/agentmanager.h"
#include "core/agentoutputmodel.h"

int main(int argc, char *argv[])
{
    KIconTheme::initTheme();
    QApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("kductor");
    QApplication::setStyle(QStringLiteral("breeze"));
    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));

    KAboutData about(
        QStringLiteral("kductor"),
        i18n("Kductor"),
        QStringLiteral(KDUCTOR_VERSION_STRING),
        i18n("AI Agent Orchestrator"),
        KAboutLicense::MIT,
        i18n("(c) 2026")
    );
    about.setDesktopFileName(QStringLiteral("org.kde.kductor"));
    KAboutData::setApplicationData(about);
    QIcon appIcon = QIcon::fromTheme(QStringLiteral("kductor"));
    if (appIcon.isNull())
        appIcon = QIcon(QStringLiteral("%1/.local/share/icons/hicolor/scalable/apps/kductor.svg").arg(QDir::homePath()));
    QApplication::setWindowIcon(appIcon);

    KCrash::initialize();
    KDBusService service(KDBusService::Unique);

    auto *gitManager = new GitManager(&app);
    auto *worktreeManager = new WorktreeManager(gitManager, &app);
    auto *workspaceModel = new WorkspaceModel(worktreeManager, &app);
    auto *agentManager = new AgentManager(&app);

    QObject::connect(worktreeManager, &WorktreeManager::workspaceCreated, workspaceModel, [=](const QString &) {
        workspaceModel->addWorkspace(worktreeManager->lastCreatedWorkspace());
    });

    // --- System tray ---
    auto *tray = new KStatusNotifierItem(QStringLiteral("kductor"), &app);
    tray->setIconByPixmap(appIcon);
    tray->setToolTipIconByPixmap(appIcon);
    tray->setCategory(KStatusNotifierItem::ApplicationStatus);
    tray->setStandardActionsEnabled(true);
    tray->setStatus(KStatusNotifierItem::Active);
    tray->setToolTipTitle(i18n("Kductor"));
    tray->setToolTipSubTitle(i18n("No agents running"));

    // Context menu actions
    auto *stopAllAction = new QAction(QIcon::fromTheme(QStringLiteral("media-playback-stop-symbolic")),
                                      i18n("Stop All Agents"), tray);
    stopAllAction->setEnabled(false);
    QObject::connect(stopAllAction, &QAction::triggered, agentManager, &AgentManager::stopAll);
    tray->contextMenu()->insertAction(tray->contextMenu()->actions().first(), stopAllAction);

    // Update tray status and menu based on active agents
    QObject::connect(agentManager, &AgentManager::activeCountChanged, tray, [=]() {
        int count = agentManager->activeCount();
        stopAllAction->setEnabled(count > 0);
        if (count > 0) {
            tray->setStatus(KStatusNotifierItem::Active);
            tray->setToolTipSubTitle(i18np("%1 agent running", "%1 agents running", count));
            stopAllAction->setText(i18np("Stop %1 Agent", "Stop All %1 Agents", count));
        } else {
            tray->setStatus(KStatusNotifierItem::Active);
            tray->setToolTipSubTitle(i18n("No agents running"));
            stopAllAction->setText(i18n("Stop All Agents"));
        }
    });

    // Flash tray on agent error
    QObject::connect(agentManager, &AgentManager::agentError, tray, [=](const QString &, const QString &) {
        tray->setStatus(KStatusNotifierItem::NeedsAttention);
    });

    // --- Notifications ---
    QObject::connect(agentManager, &AgentManager::agentFinished, &app, [=](const QString &agentId) {
        if (!agentManager->notifyOnComplete())
            return;
        QString name = agentManager->agentName(agentId);
        if (name.isEmpty())
            name = i18n("Agent");
        auto *notif = new KNotification(QStringLiteral("agentCompleted"));
        notif->setTitle(i18n("Agent Completed"));
        notif->setText(i18n("%1 has finished.", name));
        notif->setIconName(QStringLiteral("kductor"));
        notif->sendEvent();
    });

    QObject::connect(agentManager, &AgentManager::agentError, &app, [=](const QString &agentId, const QString &error) {
        QString name = agentManager->agentName(agentId);
        if (name.isEmpty())
            name = i18n("Agent");
        auto *notif = new KNotification(QStringLiteral("agentError"));
        notif->setTitle(i18n("Agent Error"));
        notif->setText(i18n("%1: %2", name, error));
        notif->setIconName(QStringLiteral("kductor"));
        notif->sendEvent();
    });

    // --- QML engine ---
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    engine.rootContext()->setContextProperty(QStringLiteral("GitManager"), gitManager);
    engine.rootContext()->setContextProperty(QStringLiteral("WorktreeManager"), worktreeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("WorkspaceModel"), workspaceModel);
    engine.rootContext()->setContextProperty(QStringLiteral("AgentManager"), agentManager);

    qmlRegisterType<AgentOutputModel>("org.kde.kductor", 1, 0, "AgentOutputModel");

    engine.loadFromModule("org.kde.kductor", "Main");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    // Connect tray left-click to toggle the QML window
    auto *rootWindow = qobject_cast<QWindow *>(engine.rootObjects().first());
    if (rootWindow) {
        QObject::connect(tray, &KStatusNotifierItem::activateRequested, rootWindow, [rootWindow](bool, const QPoint &) {
            if (rootWindow->isVisible()) {
                rootWindow->hide();
            } else {
                rootWindow->show();
                rootWindow->raise();
                rootWindow->requestActivate();
            }
        });
    }

    return app.exec();
}
