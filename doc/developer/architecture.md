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
        └── ScriptHost (thread) ........ python3 in a sandbox
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
| `WebLink` | `weblink.*` | Websites as documents (`Kind::Website`): reads the address out of an internet shortcut (`.url`) or a `.webloc` file, writes the `.url` files Indeks makes, and says whether a text is the address of a website. Only `http` and `https` count; `App::openExternally()` hands such an address to the desktop's browser and opens everything else as a file. |
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
| `DocumentModel` | `documentmodel.*` | The list of documents: the scope from the sidebar, narrowed by the search and filters. Keeps the selection by document id and the documents that linger in a list. There are two: `App.documents` is the list in the window, and `App.linkCandidates` is what the dialog for linked documents finds for what is typed there. The second is switched off (`active`) while that dialog is closed, so that it reads nothing. A dialog that lets the user pick documents by searching can use it the same way, one dialog at a time. |
| `SearchWorker` | `searchworker.*` | Reads every list of `DocumentModel`, and runs full text searches, in a thread of its own, so that neither a click in the sidebar nor typing is held up. |
| `SidebarModel` | `sidebarmodel.*` | The entries of the sidebar's database tab, each with its key (`place:inbox`, `group:12`), query and count. The entries are listed at once; the numbers come from `CountWorker`. |
| `CountWorker` | `countworker.*` | Counts the documents behind sidebar entries in a thread of its own. |
| `ActivityModel` | `activitymodel.*` | What Indeks is doing and what waits, for the activity display. |
| `Welcome` | `welcome.*` | What a new installation is asked before anything is made: start a new database, and where, or open one that exists. Runs in `main()` before there is an `App`, with `WelcomeWindow.qml`, and notes the choice in the settings. |
| `DatabaseChoice` | `databasechoice.*` | What may be chosen as a database and what is said about a choice, for `App` and `Welcome` alike. |
| `TableModel` | `tablemodel.*` | CSV and TSV files as a table. |
| `PdfText` | `pdftext.*` | Text selection on PDF pages that snaps to the nearest text. |
| `InkCanvas` | `inkcanvas.*` | Freehand drawing on pages and pictures. |
| `CodeLayout` | `codelayout.*` | Lays out long text files a window at a time. |

## Scripts in the Flatpak

A script runs in a sandbox that shows it the installed system and the documents, read-only,
and nothing else: no network, no session bus, no display. `sandboxed()` in `scripthost.cpp`
says how that is started.

| Where | By | Notes |
| --- | --- | --- |
| Installed, AppImage | `bwrap` | The arguments are `sandboxArguments()`. The environment is the few variables of `sandboxEnvironment()`. |
| Flatpak | `flatpak-spawn --sandbox --no-network --watch-bus` | There is no bubblewrap inside a Flatpak, and a sandbox cannot make another by itself: it asks Flatpak, through the portal every Flatpak may use. |

What differs in the Flatpak, and what is done about it:

- **The environment.** What Flatpak starts gets the environment of the desktop session,
  whatever it is told (`--clear-env` changes nothing). So the script is started through
  `env -i` with the variables of `sandboxEnvironment()`.
- **How many paths.** Each path the sandbox is to show (`--sandbox-expose-path-ro-try`) is
  handed over as an open file in one message on the session bus, which on many systems
  takes 16 of them. `sandboxPathLimit()` is 12 there, and the run in
  `ScriptWorker` starts a new sandbox where the next document lies in a folder that would be
  one too many, showing the folders of the documents at hand instead of all locations.
- **Ending.** Killing `flatpak-spawn` ends what it started (`--watch-bus`).
- **Python** is the one of the runtime; the user's own packages are not there.

`librarytest` only covers the bubblewrap way: there is no `flatpak-spawn` outside a Flatpak.
To try the other, build the Flatpak without installing it and start it from the build
folder, with data of its own as [development.md](development.md) describes for a second
Indeks:

```sh
flatpak-builder --force-clean build-flatpak flatpak/io.github.n4n0gh.indeks.yml
flatpak-builder --run build-flatpak flatpak/io.github.n4n0gh.indeks.yml indeks
```

A script that tags what it finds shows whether the sandbox holds: whether it can read the
document, write to it, reach the network, and how many variables it has.

## Opening a database

Nothing of an index that was just opened is in memory, and whoever goes through the table
of documents first waits for the disk: seconds, in an index of some hundred thousand
documents. Counting goes through it even when it is in memory, about a second for the
numbers of the sidebar in an index of that size. None of this may happen in the main
thread, or the window comes up late and stands still after every change.

| What | Where it runs | Notes |
| --- | --- | --- |
| The numbers of the sidebar | `CountWorker`, the count thread | `SidebarModel::reload()` lists the entries at once (`rebuild()`) and asks for the numbers (`requestCounts()`); an entry shows the number it had until the new one is there. One round runs at a time, and after a round the next waits as long as the round took (five seconds at most), so that indexing does not keep a processor counting. The locations are counted in one statement (`Store::sourceCounts()`). |
| Every list of documents | `SearchWorker`, the search thread | See *Lists of documents* below. |
| Documents kept only for the user's work | the pool thread of `Library` | `Library::start()` calls `dropAbandonedLater()`: whether there is anything to look at (`Store::hasAbandonedCandidates()`) is found out in the background, and only then `dropAbandoned()` runs in the main thread, with the table in memory by then. |

- **`App.opening`** is true from the start, and again from `Library::databaseAboutToChange`,
  until the list (`DocumentModel::opening`) and the numbers (`SidebarModel::opening`) of the
  open database are there. `ActivityIndicator` says *Opening database…* while it is, before
  anything the library reports.
- **Switching** empties the list at `databaseAboutToChange` (`DocumentModel::clear()`): its
  rows belong to the database that is left, and the rows of the other one take a moment.
  The numbers are dropped with them, and both threads close their connection.
- **Numbers are a moment behind.** Nothing may rely on a number of the sidebar being exact
  right after a change. *Empty Trash…* is enabled by the number of the trash
  (`SidebarModel::countOf()`), instead of counting with every change of the selection.
- **What still walks the table in the main thread:** `Library::count()` where a dialog
  asks for one number, `Library::sources()` with its counts in the settings, the counts of
  groups and tags (through their own tables), and `dropAbandoned()` after the user took
  something away. They are quick once the index is in memory. A new one of these does not
  belong into anything that runs at start.

To see what a start costs, copy a large index with `cp --reflink=always` into a data folder
of its own (see [development.md](development.md)), on a file system that can do that, such
as Btrfs or XFS: the copy shares the blocks on the disk, but nothing of it is in memory, so
its first start is a cold one, every time.

## Lists of documents

`DocumentModel::load()` never reads a list itself: the query goes to the search thread,
whatever it is. A list of the documents alone (a place, a tag, a filter on what is known
about a document) is a walk through all of them, a quarter of a second for 20,000 rows of
some hundred thousand documents and a second for the duplicates among them.

- **Most lists are there at once.** After asking, `load()` waits `QuickAnswerMs` for the
  answer (`SearchHandover`), and a list that arrives in that time is shown before `load()`
  returns: whoever asked finds the list read, as when the model read it itself. In a
  database of ordinary size that is every list. Not waited for are searches through the
  text, and the first list of a database (`opening`).
- **A list that takes longer** comes with `SearchWorker::found()`. The model says `loading`
  meanwhile, the list dims what it shows, and after a moment `ActivityIndicator` says
  *Listing documents…* or *Searching…*.
- **Code must not count on the list being read** when `load()`, `setScope()` or `reload()`
  return. `selectId()` of a document that is not listed yet is remembered and done when the
  list arrives (`m_wantedId`); `PreviewPane.editOnceShown()` does the same for a new
  document that is to be written in.
- **Changes to the index do not restart a list that is on its way** (that would put it off
  for as long as documents come in): the list is read once more when it is there
  (`m_refreshWanted`). Such a refresh, and every other one after a change, is `quiet`:
  `loading` stays false, nothing dims and nothing is said.

## Searching within a document

The *Find* tab of the side panel (`FindPane`) searches the document that is shown in the
preview. The panel only asks and shows: it sets `PreviewPane.findText` and reads
`findCount`, `findIndex` and `findHit()`. The finding is the viewer's, since only the
viewer knows what it shows and how to mark it. A viewer that can be searched offers:

| Member | Meaning |
| --- | --- |
| `findable` | Whether it can be searched right now. |
| `findText` | What to find; set by `PreviewPane` with a `Binding`. |
| `findCount`, `findMore` | How many places were found; whether there are more than were counted. |
| `findIndex` | The hit the reader was brought to last, -1 for none. A new search sets it back to -1, upon which `PreviewPane` goes to the first hit. |
| `findHit(i)` | What the list says about hit `i`: `label` (page, line or cell) and `before`, `match`, `after`. Asked for line by line, as the list scrolls. |
| `showHit(i)` | Brings hit `i` into view. |

| Viewer | How |
| --- | --- |
| `TextPreview`, text as it is in the file | The places are found in `sourceText`, the whole text; `CodeView` marks those among the lines it holds (`marks`) and `select()` goes to one, fetching its lines first. |
| `TextPreview`, rendered text; `EmailPreview`; the text Indeks stored | `TextFind`, a child of the `TextArea`: searches the text as it reads on screen. |
| `DataTable` | `TableModel::findAll()`; the cells that hold the text are coloured. |
| `PdfPreview`, also for Office documents | Qt's `PdfSearchModel`, which goes through the pages in the background, so that hits come in as it goes. What it knows of a hit is read with `data()` and the roles of `QPdfSearchModel::Role`. |

`FindMarks` draws the marks over a `TextEdit`, for the stretch that is in view only. The
search is not case sensitive and is not asked for while a text is being edited. The marks
go when the *Find* tab is left: `SidePanel.findText` is empty then. Selecting another
document empties the field (`FindPane`).

## Filters put together by hand

`FilterStack` is the list of filters with its *Add Filter* menu; what they come to is its
`query`. Three places use it: the filter bar (`AdvancedSearchPanel`, as chips), the
conditions of a rule (`RuleEditor`) and a saved search (`SearchDialog`), the latter two as
lines with `MatchCount` below, which says how many documents the filters find.

Rules and saved searches store the filters beside the query, so that they can be changed
the way they were made. For a query that has none, `FilterStack.filtersOf()` takes it apart
term by term into the filters the menu would have made, and leaves the rest, or a query with
`OR` or an exclusion as a whole, as one *Search* filter.

## Documents or automation

The sidebar has two tabs, and the tab decides what the middle of the window is about
(`App.sidebarTab`, `App.automation`). It is not kept from one start to the next.

| Tab | List (first half of `centreSplit`) | Pane (second half) |
| --- | --- | --- |
| `database` | `DocumentList` | `PreviewPane` |
| `automation` | `AutomationList`: the rules or the scripts (`App.automationPage`) | `AutomationPane`: `RuleEditor` or `ScriptEditor` |

Each half is a `StackLayout`, so the border between list and pane, the choice of below or
beside and the hiding of the preview hold for both.

- **What is selected** in the automation list is the application's to know
  (`App.currentRule`, `App.currentScript`), because actions go by it: *Apply Selected Rule*,
  and `automation_delete`, whose text follows the page.
- **Document actions rest** while the automation tab is shown: `App::updateActions()` then
  takes the selection of documents for empty, so that a key like `Del` does nothing to
  documents that are out of sight. The selection itself stays; trying and running a script
  work on it.
- **Whatever goes to documents switches back:** `App::goTo()`, `showDocument()`, a new
  document, a search, the filter bar.
- **Nothing is stored before *Save*.** Each editor says whether it holds changes (`dirty`)
  and keeps them while the other page or the documents are shown. Only what would replace
  them asks first, through `AutomationPane.guarded()`: another rule or script in the same
  editor, and closing or quitting (`Main.qml`, `quitWhenSaved()`; the quit action is routed
  there as the request `file_quit`).

A property of a QML component must not be named like a signal handler, `on` and a capital
letter: `onRules` read as false wherever it was used.

## The toolbar

What the toolbar shows between its two panel buttons is a list of names in the settings
(`toolbarActions`): names of actions, and a few names that are not actions.

| Name | Is |
| --- | --- |
| `menu_add`, `menu_new` | The two buttons with a menu. |
| `separator` | A dividing line. |
| `search_field` | The search field: an action whose `displayComponent` is the field, so that the toolbar places it like a button. It is made anew whenever the list changes; `MainToolBar.searchField` is the one there is, or null. |
| `spacer` | The flexible space. `MainToolBar.rebuild()` splits the list at the first one into two `Kirigami.ActionToolBar`s, the second of which stands at the far end. |

`App::toolbarItem()` and `App::toolbarCandidates()` say what there is to choose from, for
`ToolbarSettingsPage.qml`. Actions that are to show their icon only in the toolbar say so with
`displayHint` in `Actions.qml`. What had a fixed place once (the search field, the buttons for
filters, saving a search and the preview) was added to toolbars from before by the
constructor of `App`, once (`toolbarLayout`); a name that is added to the default later
needs the same care, or only new installations get it.

## Threads and processes

- **Main thread:** the interface, `Library`, and every short database statement that the
  interface triggers.
- **Indexing thread:** scanning and storing. It only holds the database for writing in short
  batches, because the main thread writes too.
- **Search thread:** every list of `DocumentModel` and its full text searches.
- **Count thread:** the numbers of the sidebar (`CountWorker`).
- **Pool threads of `Library`:** PDF renditions of Office documents, counting the records
  of large JSON Lines files, and the look through the index at start. What uses the index
  there closes its connection when it is done: the thread is the pool's.
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
- **What is copied is not a file.** *New Document from Clipboard…* makes a document of
  Indeks' own, in `files/`, out of the text or the picture on the clipboard
  (`App::takeClipboard()`, then `Library::createDocument()`, `createDocumentFromHtml()`,
  `createPicture()` or, for text that is the address of a website, `createWebLink()`). A clipboard that holds files is turned down, for the reason above. The
  content is taken when the command is given and kept until the dialog is through, so that
  what the dialog shows is what is stored. The clipboard is read through the window system,
  with no portal and no file access in between, which is why this works the same inside
  the Flatpak.
- **HTML from elsewhere is cleaned before it is shown or kept.** `sanitizeHtml()` in
  `library.cpp` takes out what would make a viewer ask the network (pictures, scripts, style
  sheets, addresses in attributes); mail is shown through it, and
  `Library::createDocumentFromHtml()` stores only what it leaves.
- **Every function is an action.** Whatever the interface can do is a `QAction` defined in
  `App::setupActions()`, which is what puts it into the command bar.
