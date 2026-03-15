cmake_minimum_required(VERSION 3.20)
project(kductor VERSION 0.1.0 LANGUAGES CXX)

set(KF_MIN_VERSION "6.0.0")
set(QT_MIN_VERSION "6.6.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMFindQmlModule)
include(ECMQmlModule)
include(FeatureSummary)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX KDUCTOR
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kductor-version.h"
)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Quick
    Qml
    Gui
    QuickControls2
    Widgets
    Concurrent
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    Kirigami
    I18n
    CoreAddons
    Config
    DBusAddons
    Notifications
    StatusNotifierItem
    Crash
    WindowSystem
    IconThemes
)

find_package(KF6KirigamiAddons 1.0.0 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED IMPORTED_TARGET libgit2)

ecm_find_qmlmodule(org.kde.kirigami REQUIRED)
ecm_find_qmlmodule(org.kde.kirigamiaddons.formcard REQUIRED)

add_subdirectory(src)

install(PROGRAMS org.kde.kductor.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.kductor.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
install(FILES icons/kductor.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
