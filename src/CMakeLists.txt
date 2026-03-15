add_executable(kductor
    main.cpp
    core/gitmanager.cpp
    core/worktreemanager.cpp
    core/workspace.cpp
    core/workspacemodel.cpp
    core/agentprocess.cpp
    core/agentmanager.cpp
    core/agentoutputmodel.cpp
)

ecm_add_qml_module(kductor
    URI org.kde.kductor
    QML_FILES
        ui/Main.qml
        ui/DashboardPage.qml
        ui/WorkspacePage.qml
        ui/WorkspaceCreateSheet.qml
        ui/AgentPanel.qml
        ui/DiffViewerPage.qml
        ui/SettingsPage.qml
        ui/AboutPage.qml
        ui/components/WorkspaceCard.qml
        ui/components/AgentStatusCard.qml
        ui/components/StatusBadge.qml
        ui/components/StreamingTextArea.qml
        ui/components/CommandBar.qml
)

target_include_directories(kductor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(kductor PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Qml
    Qt6::Gui
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::Concurrent
    KF6::Kirigami
    KF6::I18n
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::DBusAddons
    KF6::Notifications
    KF6::StatusNotifierItem
    KF6::Crash
    KF6::WindowSystem
    KF6::IconThemes
    PkgConfig::LIBGIT2
)

install(TARGETS kductor ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
