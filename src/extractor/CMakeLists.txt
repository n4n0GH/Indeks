# SPDX-FileCopyrightText: 2026 n4n0
# SPDX-License-Identifier: CC0-1.0

# The helper that reads files for the index in a process of its own.
add_executable(indeks-extractor main.cpp)
target_link_libraries(indeks-extractor PRIVATE indekscore Qt6::Core)
install(TARGETS indeks-extractor DESTINATION ${KDE_INSTALL_LIBEXECDIR})
