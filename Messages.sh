#! /usr/bin/env bash
# SPDX-FileCopyrightText: 2026 n4n0
# SPDX-License-Identifier: CC0-1.0

# Collects the translatable strings into $podir/indeks.pot.
$EXTRACTRC $(find src -name '*.kcfg') >> rc.cpp
$XGETTEXT $(find src -name '*.cpp' -o -name '*.h' -o -name '*.qml') rc.cpp -o "$podir/indeks.pot"
rm -f rc.cpp
