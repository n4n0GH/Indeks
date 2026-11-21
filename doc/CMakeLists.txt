# SPDX-FileCopyrightText: 2026 n4n0
# SPDX-License-Identifier: CC0-1.0

# The manual: a PDF made from the chapters in manual/, by a tool that is built here for it.
# It is made whenever Indeks is built, so it is always as its chapters are, and it is
# installed with Indeks, which opens it for "Indeks Manual" (F1).

add_executable(indeks-manualbuilder tools/manualbuilder.cpp)
target_link_libraries(indeks-manualbuilder PRIVATE Qt6::Gui)

# In the order of their names, which begin with a number for that.
file(GLOB manual_chapters CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/manual/*.md")
list(SORT manual_chapters)
file(GLOB manual_pictures CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/manual/images/*")

set(manual_pdf "${CMAKE_CURRENT_BINARY_DIR}/Indeks-Manual.pdf")
add_custom_command(
    OUTPUT "${manual_pdf}"
    COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
            $<TARGET_FILE:indeks-manualbuilder>
            --output "${manual_pdf}"
            --name Indeks
            --release "${PROJECT_VERSION}"
            --logo "${CMAKE_SOURCE_DIR}/io.github.n4n0gh.indeks.svg"
            ${manual_chapters}
    DEPENDS indeks-manualbuilder ${manual_chapters} ${manual_pictures} "${CMAKE_SOURCE_DIR}/io.github.n4n0gh.indeks.svg"
    COMMENT "Making the manual"
    VERBATIM
)
add_custom_target(manual ALL DEPENDS "${manual_pdf}")

install(FILES "${manual_pdf}" DESTINATION "${KDE_INSTALL_DATADIR}/indeks")
