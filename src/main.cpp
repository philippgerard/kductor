#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedContext>
#include <KLocalizedString>
#include <KCrash>
#include <KIconTheme>
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
        KAboutLicense::GPL_V3,
        i18n("(c) 2026")
    );
    about.setDesktopFileName(QStringLiteral("org.kde.kductor"));
    KAboutData::setApplicationData(about);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("kductor")));

    KCrash::initialize();
    KDBusService service(KDBusService::Unique);

    auto *gitManager = new GitManager(&app);
    auto *worktreeManager = new WorktreeManager(gitManager, &app);
    auto *workspaceModel = new WorkspaceModel(worktreeManager, &app);
    auto *agentManager = new AgentManager(&app);

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

    return app.exec();
}
