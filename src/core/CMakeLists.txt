# SPDX-FileCopyrightText: 2026 n4n0
# SPDX-License-Identifier: CC0-1.0

add_library(indekscore STATIC)

target_sources(indekscore PRIVATE
    canvas.cpp
    canvas.h
    textformat.cpp
    textformat.h
    weblink.cpp
    weblink.h
    database.cpp
    database.h
    databasefolder.cpp
    databasefolder.h
    delimited.cpp
    delimited.h
    email.cpp
    email.h
    excerpt.cpp
    excerpt.h
    extractor.cpp
    extractor.h
    fingerprint.cpp
    fingerprint.h
    extractorpool.cpp
    extractorpool.h
    indexer.cpp
    indexer.h
    indexing.cpp
    indexing.h
    jsonformat.cpp
    jsonformat.h
    library.cpp
    library.h
    mailbox.cpp
    mailbox.h
    maintenance.cpp
    maintenance.h
    ocr.cpp
    ocr.h
    office.cpp
    office.h
    paths.cpp
    paths.h
    remotescanner.cpp
    remotescanner.h
    rules.cpp
    rules.h
    scripthost.cpp
    scripthost.h
    scripts.cpp
    scripts.h
    searchquery.cpp
    searchquery.h
    store.cpp
    store.h
    task.cpp
    task.h
    todocalendar.cpp
    todocalendar.h
    types.cpp
    types.h
)

# The Python program that runs the user's scripts is compiled in as a string.
file(READ scriptbridge.py INDEKS_SCRIPT_BRIDGE)
configure_file(scriptbridge.h.in ${CMAKE_CURRENT_BINARY_DIR}/scriptbridge.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS scriptbridge.py)

ecm_qt_declare_logging_category(indekscore
    HEADER indeks_core_debug.h
    IDENTIFIER INDEKS_CORE
    CATEGORY_NAME io.github.n4n0gh.indeks.core
    DESCRIPTION "Indeks (index and library)"
    EXPORT INDEKS
)

set_target_properties(indekscore PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)

# Indeks::Library is used from QML; the application registers it from these meta types.
qt_extract_metatypes(indekscore)

target_include_directories(indekscore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(indekscore
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Sql
        Qt6::Qml
        KF6::CoreAddons
        KF6::KIOCore
    PRIVATE
        Qt6::Pdf
        KF6::I18n
        KF6::FileMetaData
        KF6::Mime
)

# Where the helper that reads files is installed; the program looks beside itself first.
target_compile_definitions(indekscore PRIVATE INDEKS_LIBEXEC_DIR="${KDE_INSTALL_FULL_LIBEXECDIR}")
