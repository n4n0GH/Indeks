# SPDX-FileCopyrightText: 2026 n4n0
# SPDX-License-Identifier: CC0-1.0

ecm_add_tests(
    searchquerytest.cpp
    delimitedtest.cpp
    textformattest.cpp
    librarytest.cpp
    LINK_LIBRARIES indekscore Qt6::Test Qt6::Gui
)
