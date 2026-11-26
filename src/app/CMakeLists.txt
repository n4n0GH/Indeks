# SPDX-FileCopyrightText: 2026 n4n0
# SPDX-License-Identifier: CC0-1.0

add_executable(indeks)

ecm_add_qml_module(indeks
    URI io.github.n4n0gh.indeks
    DEPENDENCIES
        QtQuick
        org.kde.kirigamiaddons.statefulapp
)

target_sources(indeks PRIVATE
    activitymodel.cpp
    activitymodel.h
    app.cpp
    app.h
    codelayout.cpp
    codelayout.h
    countworker.cpp
    countworker.h
    databasechoice.cpp
    databasechoice.h
    documentmodel.cpp
    documentmodel.h
    inkcanvas.cpp
    inkcanvas.h
    main.cpp
    panellayout.cpp
    panellayout.h
    pdftext.cpp
    pdftext.h
    searchworker.cpp
    searchworker.h
    sidebarmodel.cpp
    sidebarmodel.h
    systemcalendar.cpp
    systemcalendar.h
    tablemodel.cpp
    tablemodel.h
    thumbnailprovider.cpp
    thumbnailprovider.h
    welcome.cpp
    welcome.h
)

kconfig_target_kcfg_file(indeks
    FILE indekssettings.kcfg
    CLASS_NAME Config
    MUTATORS
    DEFAULT_VALUE_GETTERS
    GENERATE_PROPERTIES
    GENERATE_MOC
    QML_REGISTRATION
    SINGLETON
)

file(GLOB indeks_qml_files CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} qml/*.qml)
ecm_target_qml_sources(indeks SOURCES ${indeks_qml_files})

# Lets QML see Indeks::Library, which lives in the core library.
qt_generate_foreign_qml_types(indekscore indeks)

target_include_directories(indeks PRIVATE ${CMAKE_BINARY_DIR}/src)
# Where the manual is made (see doc/), for opening it when Indeks is run without installing.
target_compile_definitions(indeks PRIVATE INDEKS_BUILT_MANUAL="${CMAKE_BINARY_DIR}/doc/Indeks-Manual.pdf")

# The logo is part of the program, so that it has its icon also where it is not installed:
# run from the build folder, or under a desktop that does not look into the icon theme.
qt_add_resources(indeks "logo"
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}"
    FILES "${CMAKE_SOURCE_DIR}/io.github.n4n0gh.indeks.svg"
)

target_link_libraries(indeks PRIVATE
    indekscore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::DBus
    Qt6::Pdf
    KF6::I18n
    KF6::I18nQml
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::Crash
    KF6::DBusAddons
    KF6::IconThemes
    KF6::KIOGui
    KF6::KIOWidgets
    KF6::GuiAddons
    KF6::Service
    KF6::WindowSystem
    KirigamiAddonsStatefulApp
)

install(TARGETS indeks ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
