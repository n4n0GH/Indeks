# SPDX-FileCopyrightText: 2026 n4n0
# SPDX-License-Identifier: CC0-1.0

cmake_minimum_required(VERSION 3.20)

project(indeks VERSION 0.5.1 LANGUAGES CXX)

set(QT_MIN_VERSION "6.8.0")
set(KF_MIN_VERSION "6.8.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMQmlModule)
include(ECMAddTests)
include(ECMInstallIcons)
include(ECMQtDeclareLoggingCategory)
include(ECMFindQmlModule)
include(FeatureSummary)

ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX INDEKS
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/indeks-version.h"
)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Qml
    Quick
    QuickControls2
    Sql
    Network
    DBus
    Pdf
    Test
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    Kirigami
    I18n
    CoreAddons
    Config
    KIO
    Crash
    DBusAddons
    IconThemes
    FileMetaData
    WindowSystem
    Service
    GuiAddons
)
find_package(KF6Mime ${KF_MIN_VERSION} REQUIRED)
find_package(KF6KirigamiAddons 1.4.0 REQUIRED)

# Runtime-only dependencies.
ecm_find_qmlmodule(org.kde.kirigami REQUIRED)
ecm_find_qmlmodule(org.kde.kirigamiaddons.formcard REQUIRED)
ecm_find_qmlmodule(org.kde.kirigamiaddons.statefulapp REQUIRED)
ecm_find_qmlmodule(org.kde.syntaxhighlighting REQUIRED)
ecm_find_qmlmodule(QtQuick.Pdf REQUIRED)

set_package_properties(Qt6Pdf PROPERTIES TYPE REQUIRED PURPOSE "PDF preview and rendering of pages for OCR")
set_package_properties(KF6FileMetaData PROPERTIES TYPE REQUIRED PURPOSE "Text and metadata extraction for indexing")
set_package_properties(KF6Mime PROPERTIES TYPE REQUIRED PURPOSE "Reading exported e-mails")

add_definitions(-DTRANSLATION_DOMAIN=\"indeks\")
add_definitions(-DQT_NO_KEYWORDS -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII -DQT_NO_URL_CAST_FROM_STRING)

add_subdirectory(src)
add_subdirectory(doc)
if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()

install(PROGRAMS io.github.n4n0gh.indeks.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES io.github.n4n0gh.indeks.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES io.github.n4n0gh.indeks.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/po")
    ki18n_install(po)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
