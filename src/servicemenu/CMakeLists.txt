# SPDX-FileCopyrightText: 2026 n4n0
# SPDX-License-Identifier: CC0-1.0

# "Send to Indeks" in Dolphin's context menu, on folders: a folder becomes an indexed location.
# Files are not offered it; they come in through the folders they are in. Service menus have
# to be executable.
install(PROGRAMS io.github.n4n0gh.indeks.sendto.desktop DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus)
