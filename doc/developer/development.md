<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Development

## Building

Requirements (package names for openSUSE Tumbleweed in brackets):

- CMake, Ninja, a C++20 compiler, Extra CMake Modules
- Qt 6.8 or later: Quick, Quick Controls, Sql (SQLite with FTS5), Pdf [`qt6-pdf-devel`,
  `qt6-pdfquick-devel`, `qt6-pdf-imports`]
- KDE Frameworks 6.8 or later: Kirigami, I18n, CoreAddons, Config, KIO, Service, GuiAddons,
  Crash, DBusAddons, IconThemes, WindowSystem, FileMetaData [`kf6-kfilemetadata-devel`],
  KMime [`kf6-kmime-devel`]
- Kirigami Addons 1.4 or later [`kirigami-addons6-devel`]
- At run time: the `org.kde.syntaxhighlighting` QML module, and Tesseract for text
  recognition [`tesseract-ocr`, `tesseract-ocr-traineddata-<language>`]
- Optional at run time: LibreOffice [`libreoffice-writer`, `libreoffice-calc`,
  `libreoffice-impress`], Python 3 [`python3`] and bubblewrap [`bubblewrap`]

```sh
cmake -S . -B build -G Ninja -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

Building also makes the manual, `build/doc/Indeks-Manual.pdf`.

Installing is needed for the entry in the application menu, the icon in the icon theme and
*Send to Indeks* in Dolphin. To try Indeks without installing, run `build/bin/indeks`.

```sh
indeks                      # start, or raise the running window
indeks report.pdf notes.md  # put files into the inbox
indeks ~/Documents          # index a folder
```

## Layout of the sources

| Directory | Content |
| --- | --- |
| `src/core` | The library: index, search language, reading files, text recognition, indexer, remote scanner, rules, scripts, `Library` |
| `src/extractor` | The helper program that reads files in a process of its own |
| `src/app` | The application: actions, list models, drawing, and the QML interface in `qml/` |
| `src/servicemenu` | *Send to Indeks* for Dolphin |
| `autotests` | Tests of the search language, the table reader and the library |
| `doc/manual` | The manual, a chapter per file |
| `doc/developer` | This documentation |
| `doc/tools` | The program that makes the PDF of the manual |
| `flatpak` | The Flatpak manifest and what publishes the Flatpak repository |
| `packaging/appimage` | The script that builds the AppImage |
| `.github/workflows` | Building and publishing both for every release; see [releasing.md](releasing.md) |

See [architecture.md](architecture.md) for what is in them.

## Tests

`ctest` runs four tests:

- `appstreamtest` checks the metadata file.
- `searchquerytest` checks what the search language compiles to.
- `delimitedtest` checks the reading of CSV and TSV.
- `librarytest` drives the core through `Library` with real files in temporary folders:
  indexing, moved and deleted files, mailboxes, rules, scripts, to-dos, the trash. It needs
  no display. Tests that need Tesseract, LibreOffice, Python or bubblewrap are skipped where
  those are missing.

`librarytest` shares one library and a set of fixture files among its test functions, which
run in the order they are written in. A test that needs files of its own, or that leaves
something behind, brings and removes its own in a temporary folder.

Useful environment variables:

| Variable | Effect |
| --- | --- |
| `INDEKS_DATA_DIR` | The folder of the local database, instead of `~/.local/share/indeks`. The tests set it. |
| `INDEKS_EXTRACTOR_TIMEOUT_MS` | How long a helper may take over one file. |
| `QT_FORCE_STDERR_LOGGING=1` | Makes Qt write its messages, QML errors included, to standard error when that is not a terminal. |

## Trying the interface without touching your own data

Indeks is one instance per session: a second start only hands its arguments to the first. To
run a second, independent Indeks, give it its own data, settings and D-Bus session:

```sh
INDEKS_DATA_DIR=/tmp/indeks-try/data XDG_CONFIG_HOME=/tmp/indeks-try/config \
    dbus-run-session -- build/bin/indeks
```

## Conventions

- **KDE style.** The code is built with KDE's compiler settings: string literals are
  `u"…"_s` or `"…"_L1`, signals are emitted with `Q_EMIT`, no implicit conversions from
  `char *`.
- **Every function is an action.** Add it in `App::setupActions()` with `addAction()`,
  `addRequest()` (handled in `Main.qml`) or `addToggle()`. That puts it into the command bar
  and lets it be given a shortcut. Give it a property in `Actions.qml` if QML shows it.
- **Texts for people** go through `i18nc()` with a context, and follow the KDE Human
  Interface Guidelines: title case for commands, an ellipsis (`…`) where a command asks
  for more before it acts.
- **Settings** are entries of `src/app/indekssettings.kcfg`; QML reads them as `Config`.
  What belongs to a database rather than to the computer goes into the index instead.
- **Comments** say why, not what. Files carry SPDX lines for copyright and licence: code is
  GPL-2.0-or-later, build files and texts are CC0-1.0.
- **Nothing destructive.** Code never writes to an indexed file and never deletes a file of
  the user's. Removing something from the index never removes the file it stands for.

## Adding a feature: a checklist

1. The work itself goes into `src/core`, behind `Library`, with a test in `librarytest`.
2. An action in `App::setupActions()`; its state in `App::updateActions()`.
3. The interface in `src/app/qml`.
4. If the database changes: see [database.md](database.md).
5. The manual: see [documentation.md](documentation.md). A feature is not done before the
   manual describes it and the chapter *Changes* lists it.
