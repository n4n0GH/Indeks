# SPDX-FileCopyrightText: 2026 n4n0
# SPDX-License-Identifier: CC0-1.0

add_subdirectory(core)
add_subdirectory(extractor)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app/CMakeLists.txt")
    add_subdirectory(app)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/servicemenu/CMakeLists.txt")
    add_subdirectory(servicemenu)
endif()
