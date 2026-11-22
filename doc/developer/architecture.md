<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Architecture

Indeks is a Qt 6 application with a QML interface (Kirigami) on top of a C++ library that
does the work. It is built as three programs and one library.

```
 QML interface (src/app/qml)
        │  App, DocumentModel, SidebarModel, ActivityModel …        src/app
        ▼
     Library ───────────────────────────────────────────────────    src/core
        │         the one object the interface talks to
        ├── Store, Database ............ SQLite index (index.sqlite)
        ├── Indexer (thread) ........... scans folders, stores what was read
        │      └── ExtractorPool ─────── indeks-extractor processes   src/extractor
        ├── RemoteScanner .............. KIO: lists and downloads remote files
        ├── Ocr ........................ tesseract processes
        ├── Office ..................... LibreOffice, for PDF renditions
        ├── Rules ...................... filters and actions
        └── ScriptHost (thread) ........ python3 in a bubblewrap sandbox
```

## The parts

| Target | Directory | What it is |
| --- | --- | --- |
| `indekscore` | `src/core` | Static library with everything that is not interface. No QML, no widgets; tested without a display. |
| `indeks` | `src/app` | The application: actions, list models, the QML interface. |
| `indeks-extractor` | `src/extractor` | Helper program that reads one file after the other and reports what it found. Run several at once by `ExtractorPool`. |
| `indeks-manualbuilder` | `doc/tools` | Makes the PDF of the manual at build time. Not installed. |
| *Send to Indeks* | `src/servicemenu` | A service menu for Dolphin, offered for folders, that starts `indeks` with them. |

## Core classes

| Class or namespace | File | Role |
| --- | --- | --- |
| `Library` | `library.*` | The interface of the core. Everything the application does to documents goes through it, and it says what changed with signals (`changed`, `documentChanged`, `todosChanged`, …). Lives in the main thread. |
| `Database` | `database.*` | Opens the SQLite file, one connection per thread, and brings older databases up to the current schema (`SchemaVersion`). Tells an index of Indeks from another file of its name (`inspectFile()`, `ApplicationId`). |
| `Store` | `store.*` | Reading and writing of documents and what belongs to them, as plain functions on the current thread's connection. |
| `SearchQuery` | `searchquery.*` | Compiles the search language into an SQL condition and an FTS5 expression. |
| `Indexer`, `IndexWorker` | `indexer.*` | The indexing thread: scans locations, hands files to the extractor pool, stores the results, takes mailboxes apart, queues text recognition. |
| `Indexing` | `indexing.*` | What happens to one file: what to read of it, and how the result is stored. Shared by the indexer and the helper program. |
| `ExtractorPool` | `extractorpool.*` | Runs the helper programs, feeds them files, and survives their crashes and hang-ups. |
| `extractContent()` | `extractor.*` | Gets text and metadata out of a file, by type. Runs inside the helper program. |
| `RemoteScanner` | `remotescanner.*` | Indexes locations reached through KIO. Downloads one file at a time into the cache. |
| `Mailbox` | `mailbox.*` | Goes through an mbox file once and notes where each message stands. |
| `Ocr` | `ocr.*` | Runs Tesseract on pictures and on pages of PDFs. |
| `Office` | `office.*` | Has LibreOffice make PDF renditions of Office documents. |
| `Rules` | `rules.*` | Which documents a rule matches, and carrying out its actions. Used from the main thread and the script thread. |
| `Scripts` | `scripts.*` | The script files of a database, and which of them may run on this computer. |
| `ScriptHost` | `scripthost.*` | Runs scripts: a thread that talks to `python3` in a sandbox, in lines of JSON. `scriptbridge.py` is the other end, compiled into the library. |
| `DatabaseFolder` | `databasefolder.*` | A database as a folder: whether it can be reached, whether it holds an index of Indeks, something else of that name or nothing at all, whether another computer has it open. |
| `Maintenance` | `maintenance.*` | Gives space back to the file system after documents were removed. |
| `Paths` | `paths.*` | Where things are: database folder, cache. Honours `INDEKS_DATA_DIR`. |

## Application classes

| Class | File | Role |
| --- | --- | --- |
| `App` | `app.*` | The application object (`AbstractKirigamiApplication`). Defines every action, keeps the settings in step, and is what QML reaches as `App`. |
| `DocumentModel` | `documentmodel.*` | The list of documents: the scope from the sidebar, narrowed by the search and filters. Keeps the selection by document id and the documents that linger in a list. |
| `SearchWorker` | `searchworker.*` | Runs full text searches in a thread of their own, so that typing is never held up. |
| `SidebarModel` | `sidebarmodel.*` | The entries of the sidebar, each with its key (`place:inbox`, `group:12`), query and count. |
| `ActivityModel` | `activitymodel.*` | What Indeks is doing and what waits, for the activity display. |
| `Welcome` | `welcome.*` | What a new installation is asked before anything is made: start a new database, and where, or open one that exists. Runs in `main()` before there is an `App`, with `WelcomeWindow.qml`, and notes the choice in the settings. |
| `DatabaseChoice` | `databasechoice.*` | What may be chosen as a database and what is said about a choice, for `App` and `Welcome` alike. |
| `TableModel` | `tablemodel.*` | CSV and TSV files as a table. |
| `PdfText` | `pdftext.*` | Text selection on PDF pages that snaps to the nearest text. |
| `InkCanvas` | `inkcanvas.*` | Freehand drawing on pages and pictures. |
| `CodeLayout` | `codelayout.*` | Lays out long text files a window at a time. |

## Threads and processes

- **Main thread:** the interface, `Library`, and every short database statement that the
  interface triggers.
- **Indexing thread:** scanning and storing. It only holds the database for writing in short
  batches, because the main thread writes too.
- **Search thread:** full text searches of `DocumentModel`.
- **Script thread:** `ScriptHost`.
- **Helper processes:** `indeks-extractor` (several), `tesseract` (several), `soffice`,
  `python3` inside `bwrap`. A helper that crashes or hangs is replaced; the file it was at is
  indexed by name.

Each thread has its own SQLite connection. The database runs in WAL mode on local storage
and with a classic journal on network shares.

## Principles the code keeps to

- **Indexed files are never written to.** Editing and drawing work on a copy in the
  database folder (`copies/`). Tests check this.
- **The interface never waits.** Anything that can take long runs in a thread or a process,
  and can be stopped: closing the window ends all of it within moments.
- **What the user attached is never dropped silently.** A document with user data survives
  its file and its location; see `Store::hasUserData()`.
- **Files come in through locations.** There is no way to add a single file:
  `Library::addFolders()`, where drops, *Send to Indeks* and the command line end up, makes
  folders locations and turns files down with a message. A file that is indexed where it
  happens to lie is not seen to belong to Indeks, and is easily deleted by a user who is
  tidying up. Documents with `source_id` 0 that are not Indeks' own are from before this
  rule, or were kept when their location was removed.
- **Every function is an action.** Whatever the interface can do is a `QAction` defined in
  `App::setupActions()`, which is what puts it into the command bar.
