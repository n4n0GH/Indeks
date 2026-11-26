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
| `TextFormat` | `textformat.*` | Says what a plain text file is written as, going by its beginning: Markdown, values in rows and columns, or text (`guess()`). Careful on purpose, since prose taken for Markdown loses its line breaks; `textformattest` holds the cases on both sides of the line. `Library::guessedTextFormat()` asks it for `text/plain` files only, and `TextPreview` shows the file as what it is, with the raw view one click away. Tables are told with the help of `DelimitedTable` (`delimited.*`), whose `Dialect` also covers bars between spaces (`a | b`) and rows that go on in lines set in below them (`guessDialect()`, `looksLikeSpacedBars()`). |
| `WebLink` | `weblink.*` | Websites as documents (`Kind::Website`): reads the address out of an internet shortcut (`.url`) or a `.webloc` file, writes the `.url` files Indeks makes, and says whether a text is the address of a website. Only `http` and `https` count; `App::openExternally()` hands such an address to the desktop's browser and opens everything else as a file. |
| `Ocr` | `ocr.*` | Runs Tesseract on pictures and on pages of PDFs. |
| `Office` | `office.*` | Has LibreOffice make PDF renditions of Office documents. |
| `Fingerprint` | `fingerprint.*` | What a picture or the first page of a PDF looks like, in 256 bits, and the comparison of two of them, in the program and as SQL. See *Documents that look alike* below. |
| `Rules` | `rules.*` | Which documents a rule matches, and carrying out its actions. Used from the main thread and the script thread. |
| `Scripts` | `scripts.*` | The script files of a database, and which of them may run on this computer. |
| `ScriptHost` | `scripthost.*` | Runs scripts: a thread that talks to `python3` in a sandbox, in lines of JSON. `scriptbridge.py` is the other end, compiled into the library. |
| `DatabaseFolder` | `databasefolder.*` | A database as a folder: whether it can be reached, whether it holds an index of Indeks, something else of that name or nothing at all, whether another computer has it open. |
| `Maintenance` | `maintenance.*` | Gives space back to the file system after documents were removed. |
| `TodoCalendar` | `todocalendar.*` | The to-dos that have a deadline as a calendar file (iCalendar), and writing it. See *To-dos in the calendar* below. |
| `Paths` | `paths.*` | Where things are: database folder, cache. Honours `INDEKS_DATA_DIR`. |

## Application classes

| Class | File | Role |
| --- | --- | --- |
| `App` | `app.*` | The application object (`AbstractKirigamiApplication`). Defines every action, keeps the settings in step, and is what QML reaches as `App`. |
| `DocumentModel` | `documentmodel.*` | The list of documents: the scope from the sidebar, narrowed by the search and filters. Keeps the selection by document id and the documents that linger in a list. There are two: `App.documents` is the list in the window, and `App.linkCandidates` is what the dialog for linked documents finds for what is typed there. The second is switched off (`active`) while that dialog is closed, so that it reads nothing. A dialog that lets the user pick documents by searching can use it the same way, one dialog at a time. |
| `SearchWorker` | `searchworker.*` | Reads every list of `DocumentModel`, and runs full text searches, in a thread of its own, so that neither a click in the sidebar nor typing is held up. |
| `PanelLayout` | `panellayout.*` | Which view is in which area of the sidebar and the side panel, and which view each area shows; see *The two panels and their views*. |
| `SystemCalendar` | `systemcalendar.*` | Puts the calendar file of the to-dos among the calendars of Akonadi, and takes it out, over D-Bus. See *To-dos in the calendar* below. |
| `SidebarModel` | `sidebarmodel.*` | The entries of the sidebar's database tab, each with its key (`place:inbox`, `group:12`), query and count. The entries are listed at once; the numbers come from `CountWorker`. The sections with a heading stand in the order the user gave them (`sectionOrder`, `moveSection()`; the setting `sidebarSections`). |
| `CountWorker` | `countworker.*` | Counts the documents behind sidebar entries in a thread of its own. |
| `ActivityModel` | `activitymodel.*` | What Indeks is doing and what waits, for the activity display. |
| `Welcome` | `welcome.*` | What a new installation is asked before anything is made: start a new database, and where, or open one that exists. Runs in `main()` before there is an `App`, with `WelcomeWindow.qml`, and notes the choice in the settings. |
| `DatabaseChoice` | `databasechoice.*` | What may be chosen as a database and what is said about a choice, for `App` and `Welcome` alike. |
| `ThumbnailProvider` | `thumbnailprovider.*` | The small pictures of the grid of documents, `image://thumbnail/…`: made in a pool of its own (three threads), kept as PNG files in the cache folder. See *The documents as list and as grid* below. |
| `TableModel` | `tablemodel.*` | CSV and TSV files as a table. Rows and columns are counted as the table is shown: without the columns the user hid (`m_shown`), in the order the rows are sorted into (`m_order`, worked out in a pool thread by `sortBy()`). The file and `DelimitedTable` know nothing of either. |
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
| Thumbnails of the grid | `ThumbnailProvider`, a pool of three threads | Reads the files themselves, never the index. |
| What documents look like | the indexing thread and its helper processes | `IndexWorker::lookAtDocuments()`; the walk through the table that finds what is still to be looked at runs there too. |
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

## Documents that look alike

`similar:<id>` in the query language finds the documents that look like one of them
(`SearchQuery`, `Fingerprint`). It is there for files whose names say nothing: the invoices
of one sender among a scanner's output.

- **What is compared.** `Fingerprint::ofImage()` decides between two shapes. A *page* (60 %
  of it light, and something in at least 8 of the fields of its head) is known by its upper
  quarter: 32 by 8 fields, a bit for each that is darker than the average. A *picture*,
  which is everything else, is known as a whole: 16 by 16 fields, a bit for each that is
  lighter than its left neighbour. Only fingerprints of one shape are compared, by the
  number of bits they differ in; how many may differ (`allowance()`) depends on the shape,
  on *roughly*, and on how many bits the two have set at all.
- **The numbers** in `fingerprint.cpp` come from looking at some hundred real invoices and
  pictures side by side with their nearest neighbours, not from theory. `indeks-extractor
  --look <files>` prints the fingerprints of files, for doing that again.
- **Where it is made.** `Indexing::readFile()` takes it when it reads a picture or a PDF,
  in the helper process like the rest of the reading, and `storeFile()` notes it
  (`Prepared::look`). Documents from before are looked at by
  `IndexWorker::lookAtDocuments()`, which `Library::start()` asks for: it collects the
  pictures and PDFs without a fingerprint of their present content and sends them through
  the pool with `Reading::lookOnly`, 200 at a time, with the event loop in between like
  text recognition. `Library::canFindSimilar()` looks at one document on the spot, the one
  the user asks about.
- **Where it is kept.** The table `fingerprints`, which is no part of the versioned schema;
  see [database.md](database.md). A row without a shape says that the document was looked
  at and gave nothing, so that it is not looked at with every start.
- **How it is compared.** In SQL, in the query of the list like every other condition, so
  that it combines with the rest of the language, in saved searches and in rules. SQLite
  has neither XOR nor a count of bits; `Fingerprint::sqlSimilar()` spells both out with
  `|`, `&`, shifts and sums, for eight numbers of 32 bits. `fingerprinttest` holds it
  against the count the program makes. For 80,000 fingerprints that takes a quarter of a
  second; it runs in the search thread.
- **In the interface** it is a filter of `FilterStack` (`similarFilter()`): the actions
  `doc_similar` and `doc_similar_roughly` put it on the filter bar, and the *Add Filter*
  menu offers it, and its opposite, for the document that is selected.

## The documents as list and as grid

`DocumentList.qml` shows the one `DocumentModel` in two ways, a `ListView` with columns and
a `GridView` of tiles; `Config.documentsAsGrid` says which. Only the view that is looked at
has the model, so the other makes no delegates. Selection, the context menu, dragging and
the keys go through the same functions of the component for both (`selectRow()`,
`moveCurrent()`, `dragProxy`), and what is said while there is nothing to show stands over
whichever is shown.

- **Both views fill the same room** (`views`), by anchors, whichever is shown. They are not
  rows of the layout that come and go with `visible`: a layout gives an item that is not
  shown no size, and a `ScrollView` that was never given one is as large as what is in it.
  A `ListView` that gets the model in that state grows with every row it makes and so
  makes a row for every document, and throws all but a screenful away once the layout has
  told it how much room there is. With twenty thousand documents that took minutes
  (0.5.0, when Indeks was started with thumbnails and the list was then shown for the
  first time). The same goes for any view that is shown only some of the time: give it
  its size by anchors, or the model only once it has one.
- **The bar above them** (`viewBar`) has the *Sort By* menu, the switch and the slider. Its
  entries are the application's actions (`sort_by_<column>`, `sort_descending`,
  `view_as_list`, `view_as_grid`) under shorter names (the component `Choice`), and
  `App::syncDocumentView()` keeps the marks of those actions with the model and the
  settings, whoever changed them.
- **A thumbnail's address** is the role `thumbnail` of the model
  (`ThumbnailProvider::address()`): number, kind, time of change and size of the document
  and the path of its file. It is empty for kinds that have none (anything but pictures,
  PDFs and plain text), for parts of other documents, for files that are not on this
  computer (`DocumentRow::path` is only set for local files) and for missing or
  unavailable ones; the tile shows the icon of the type then, as it does until the
  picture is there.
- **The provider** asks nothing of the index: all it needs is in the address. It answers
  from the cache folder (`thumbnails/<hash of path, time and size>-<edge>.png`) or makes
  the picture: `QImageReader` with a scaled size, the first page with `QPdfDocument`, the
  first 6 KiB of a text drawn on a sheet. Pictures are made at 128, 256 or 512 points and
  scaled by the tile, so that sliding the size does not make all of them anew. A request
  whose tile scrolled away is cancelled before it starts. Files not looked at for 90 days
  are removed at start (`prune()`).
- **A file that changes** gets a new address with the next list, since time and size are
  part of it; the old picture stays in the cache until it is pruned.

## The bars across the top

The rows of tabs of the two panels, the bar above the documents and the bar of the preview
are of one height: `mainPage.barHeight` in `Main.qml`, the larger of what the latter two
would take by themselves (`naturalBarHeight`), handed to all of them as `barHeight`. A bar
that is told nothing takes its own height.

A `PanelArea` with one view leaves its row of tabs out when `Config.hideSingleTabBar` is
set (`barShown`). Its drop areas stay, so another view can be dragged in; the view itself
is then moved with the actions `panel_move_<view>` and the like.

## The two panels and their views

The sidebar on the left and the side panel on the right are made the same way
(`SidePanel.qml`, with `side` "left" or "right"): an upper and a lower area
(`PanelArea.qml`), each a bar of tabs with the view of the chosen tab below it, and a
border between them that the user moves (`leftPanelLower`, `rightPanelLower`).

A **view** is what a tab leads to. There are five: `database` (`DatabaseView`),
`automation` (`AutomationView`), `info` (`InfoPane`), `todo` (`TodoPane`) and `find`
(`FindPane`). Which view is in which area of which panel is the user's to say, and
`PanelLayout` (`App.panels`) keeps it, in the settings (`panelLeftTop` and the like,
`panelCurrent`).

- **A panel slides in and out** (`PanelSlot.qml`, which is what stands in the row across
  the window; the panel is in it). Showing and hiding moves `shown` between 0 and 1, as
  long and as eased as Kirigami's drawers (`Kirigami.Units.longDuration`, which is nothing
  where the desktop has animations off), and the place is `restWidth * shown` wide
  meanwhile. The panel keeps its full width on the way and moves with the edge that faces
  the middle, so nothing in it is laid out anew; the middle is, at every step, and that is
  cheap (a few milliseconds a frame with a PDF in the preview). The width the user drags a
  panel to is `panelWidth`, not the `preferredWidth` of the split view, which follows the
  slide. The window comes up without a slide (`settled`).
- **The views are made once**, in `Main.qml`, and each puts itself into the container of
  the area it is in (`parent: mainPage.viewHome(…)`) and shows while its area has chosen it.
  So a view keeps what it holds, the text of a search for one, when it is moved.
- **Bindings follow `App.panels.revision`**, which goes up with every change; the layout is
  asked through functions (`views()`, `current()`, `sideOf()`, `isCurrent()`).
- **The upper area is the one that is always there.** `PanelLayout::normalise()` moves the
  views of a lower area up when the upper one is empty, puts a view the settings do not
  know of (one a later version brings) where it is at home, and drops what is no view. A
  panel without views is hidden (`SidePanel.hasViews`).
- **Moving is a command**, so that the command bar has it: `panel_move_<view>`,
  `panel_split_<view>` (which merges a view that is below), `panel_earlier_<view>` and
  `panel_later_<view>` for the order of the tabs, and `panel_reset`. What they are called
  follows where the view is (`App::updatePanelActions()`). The menu of a tab triggers the
  same actions.
- **Tabs are dragged** with `PanelDrag`, of which the window has one: a tab's `DragHandler`
  tells it where the pointer is, and it is what the `DropArea`s see coming: the bar of
  tabs of every area (which shows where among the tabs), the view below it (the whole
  area, or its lower part for an area of its own there), and a strip at the edge of the
  window for a panel that is not shown (`EdgeDrop` in `Main.qml`). All of them end in
  `PanelLayout::place()`, a moment after the drop, since the tabs are made anew by it.
- **A new view** needs a name in `PanelLayout` (its home, title, icon and tooltip), an
  instance in `Main.qml`, and nothing else: tabs, menus and commands follow.
- **Below the areas** of the panel that has the database view stand the database button and
  `ActivityIndicator`. The button says how large the database folder is
  (`Library::databaseSizeText`, added up in a pool thread at start and every minute).
- **`App::showSidePanel(view)`** brings a view up wherever it is and shows its panel.

## Documents or automation

What the middle of the window is about is a state of its own (`App.sidebarTab`,
`App.automation`); it is not kept from one start to the next. Two views set it: a click on
the tab or an entry of the database view makes it `database`, one on the tab or an entry of
the automation view `automation`. Since the two may be in view at the same time, in
different areas, each marks its entry only while the window is about it.

Going the other way, `App::setSidebarTab()` brings the automation view up in its area, and
the database view where the two share an area and the other one was up. A database view
that stands behind another tab is left there: a search must not take away what the user
is looking at.

| `App.sidebarTab` | List (first half of `centreSplit`) | Pane (second half) |
| --- | --- | --- |
| `database` | `DocumentList` | `PreviewPane` |
| `automation` | `AutomationList`: the rules or the scripts (`App.automationPage`) | `AutomationPane`: `RuleEditor` or `ScriptEditor` |

Each half is a `StackLayout`, so the border between list and pane, the choice of below or
beside and the hiding of the preview hold for both.

- **What is selected** in the automation list is the application's to know
  (`App.currentRule`, `App.currentScript`), because actions go by it: *Apply Selected Rule*,
  and `automation_delete`, whose text follows the page.
- **Something is always selected** while there is anything: `AutomationPane.showOne()`
  shows the first rule or script when a list comes up with none shown, and after a deletion
  the one that took its place (`rulePlace`, `scriptPlace`). Without it the actions above are
  disabled until an entry is clicked, which nobody thinks of in a list of one.
- **`Del` is the key of two commands**, `doc_trash` and `automation_delete`. They are never
  enabled together (`App::updateActions()`), so the key means the one that is. A text field
  that has the keys takes `Del` for itself.
- **Document actions rest** while the window is about automation: `App::updateActions()` then
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
| `menu_preview` | The button with the three choices for the preview (`preview_none`, `preview_below`, `preview_beside`, kept in step with the two toggles by `App::syncPreviewChoice()`). It stands for the two buttons there were; toolbars from before get it in their place, once (`toolbarLayout` 3). |
| `search_field` | The search field: an action whose `displayComponent` is the field, so that the toolbar places it like a button. It is made anew whenever the list changes; `MainToolBar.searchField` is the one there is, or null. |
| `spacer` | The flexible space. `MainToolBar.rebuild()` splits the list at the first one into two `Kirigami.ActionToolBar`s, the second of which stands at the far end. |

`App::toolbarItem()` and `App::toolbarCandidates()` say what there is to choose from, for
`ToolbarSettingsPage.qml`. Actions that are to show their icon only in the toolbar say so with
`displayHint` in `Actions.qml`. What had a fixed place once (the search field, the buttons for
filters, saving a search and the preview) was added to toolbars from before by the
constructor of `App`, once (`toolbarLayout`); a name that is added to the default later
needs the same care, or only new installations get it.

## To-dos in the calendar

The to-dos that are open and have a deadline can be shown by calendar programs. There are
two halves to it, and the first works without the second.

**The file.** `TodoCalendar::render()` makes an iCalendar file of the to-dos, one `VTODO`
each, due on its day (`DUE;VALUE=DATE`). `Library` writes it as `to-dos.ics` into the folder
of the database (`todoCalendarFile()`), if that is turned on for the database
(`todoCalendarEnabled()`: the value `todo-calendar` in `meta`).

- The file is put together anew from the list whenever `todosChanged` or `changed` says
  that something may be different (`scheduleTodoCalendar()`, once for all that comes within
  400 ms), and written only if it is not what was written last. A to-do that is done is
  simply no longer in it: nothing is ever taken out of a file.
- The same to-dos make the same bytes. `DTSTAMP` is when the to-do was made, not when the
  file was written. A calendar program that reads the file again therefore finds nothing
  changed that has not.
- A to-do is known to the calendar programs by its `UID`: its number and a random name of
  the database (`todo-calendar-id` in `meta`), so that two databases never have the same
  one in the same calendar program.
- The write is done in a pool thread of `Library` (`m_calendarPool`, one at a time), since
  the folder may be on a server. `flushTodoCalendar()` waits for it: before a database is
  closed, and when Indeks is, so that a to-do ticked off in the last moment is not left in
  the calendar.
- A file of that name that Indeks did not write (`TodoCalendar::isOwn()`, by its `PRODID`)
  is neither written over nor removed.

The file goes one way. Indeks never reads it: taking changes back from a calendar program
would need a way to tell which side changed what, and the list in the index is the one that
counts.

**The system calendar.** Akonadi, which KOrganizer, Merkuro and Plasma's clock read, shows
a calendar file through an agent of the kind `akonadi_ical_resource`. `SystemCalendar` has
such an agent made, pointed at the file, set to read only and to watch the file, and
removed again, by calling Akonadi over D-Bus (`org.freedesktop.Akonadi.Control`, the agent's
`/Settings`). Nothing of Akonadi is linked: where it is not installed, the bus has nobody
of that name, and `available` is false.

- Whether the file is shown is asked, not remembered: the agents for calendar files are
  asked for their path. It stays true when the user removes the calendar in KOrganizer.
- Akonadi is started only when the user has the calendar added, never to be asked or to be
  told to forget one. While it does not run, the state of the last time counts (`Shown` in
  the state file), and a calendar that is to go is noted (`Forget`) and removed the next
  time Akonadi is found running.
- An agent that was just made answers after a moment; `configure()` tries until it does.
- Every call is asynchronous. A `ticket` drops the answers to a question that has been
  asked anew since.
- `AKONADI_INSTANCE` is honoured (the names of the services end in it), which is how this
  is tried without touching the Akonadi of the desktop; see
  [development.md](development.md).

`App` joins the two: turning the calendar on (`setTodoCalendar()`) also adds it to the
system calendar if there is one, and turning it off takes it out there first, while the
file is still there to be found by. In the Flatpak the calls need the `--talk-name`
permissions for Akonadi in the manifest.

A calendar program other than these is given the file by the user. To write to a calendar
service instead (CalDAV, say) would be a third half, and would need Indeks to keep what it
sent where.

## Threads and processes

- **Main thread:** the interface, `Library`, and every short database statement that the
  interface triggers.
- **Indexing thread:** scanning and storing. It only holds the database for writing in short
  batches, because the main thread writes too.
- **Search thread:** every list of `DocumentModel` and its full text searches.
- **Count thread:** the numbers of the sidebar (`CountWorker`).
- **Pool threads of `Library`:** PDF renditions of Office documents, counting the records
  of large JSON Lines files, and the look through the index at start. What uses the index
  there closes its connection when it is done: the thread is the pool's. A pool of its own
  writes the calendar file of the to-dos, which does not use the index.
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
