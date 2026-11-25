<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# The database

A database is a folder. The index in it is one SQLite file, `index.sqlite`
(`Paths::indexFileName()`); beside it are `files/` (documents created in Indeks), `copies/`
(working copies) and `scripts/`.

## Whose file it is

The name alone does not make a file the index. `PRAGMA application_id` of an index is
`Database::ApplicationId` (`0x496E646B`, "Indk"); `Database::migrate()` sets it when it makes
a database, and puts it on an index from before the mark the first time that is opened. This
is no change of the schema: older versions of Indeks do not look at the number.

`Database::inspectFile()` says what a file is, without changing it:

| `FileKind` | Means | What follows |
| --- | --- | --- |
| `Missing` | No such file. | A database can be started in the folder. |
| `Blank` | An empty file, or an SQLite database without a table: a start that was cut short. | As `Missing`. |
| `Own` | It has the mark, or it has no mark and the tables `documents` and `sources`. | It is opened. |
| `Foreign` | Another mark, other tables, or no SQLite database at all. | Nothing is opened or started in the folder (`Library::databaseProblem()`). |
| `Unreadable` | An SQLite database that could not be asked, locked for instance. | Refused for now. |

`Database::connection()` asks the same question of the file it has just opened, before any
of the statements that write to it, and closes a foreign one again. That is the second line
of defence; `databaseProblem()` is asked first wherever a database is chosen.

The interface follows from this. *Add Database…* takes the index file itself
(`Library::databaseOfIndexFile()`, `App::addDatabase()`) and never starts a database. *New
Database…* takes a folder (`App::createDatabase()`), turns down one that holds a database
and warns about one that is not empty (`DatabaseFolder::State::empty`).

A new installation has neither a local database nor one in its list (`Welcome::isNeeded()`).
`main()` then shows `WelcomeWindow.qml` before it makes the `App`, so that no database is
started before the user has said where. The answer goes into the settings, and start-up
goes on as always. Two things follow from a start elsewhere than in the local folder:

- The chosen folder has no index yet when `App` picks the database to open. That one folder
  (`Welcome::folderToStartIn()`) may be started in; any other folder without an index is
  still taken for a share that is not mounted, and Indeks opens the local database.
- The local database may not exist when Indeks goes back to it. `Library::switchDatabase()`
  starts it then, whatever `create` says.

## Schema

`PRAGMA user_version` holds the schema version; `Database::SchemaVersion` is the current one.
`Database::migrate()` creates a new database and brings an older one up to date, step by
step, in one transaction. A database from a newer Indeks is refused.

| Table | Holds |
| --- | --- |
| `sources` | Indexed locations: address, name, whether subfolders are included, when last scanned. `inbox`: new files go to the inbox. `settled`: the first pass through the location is done, so what is found from now on is new. |
| `documents` | One row per document. See below. |
| `doc_fts` | FTS5 table with `name`, `content` and `notes` of each document, `rowid` = document id. |
| `metadata` | Key and value pairs found in files (author, e-mail headers, …). |
| `tags`, `document_tags` | Tags and which documents have them. |
| `folders`, `document_folders` | Groups (called folders here for historical reasons), nested through `parent_id`, and which documents are in them. |
| `document_links` | Links between documents. A link has no direction: each pair is one row, with the lower id in `a_id` and the higher in `b_id` (a `CHECK` sees to it), so a pair cannot be there twice. Asking for the links of a document means looking at both columns. |
| `searches` | Saved searches: name and query. The filters a search was put together from are in `meta`. |
| `rules` | Rules: name, the compiled query, the filters and actions as JSON, whether active and whether run on import. |
| `annotations` | Notes (`kind = 'note'`) and drawings (`kind = 'ink'`), per document and page. `x`, `y`: where a sticky note stands, as shares of the page; -1 for none. |
| `todos` | The to-do list: text, linked document (0 for none), deadline as `2026-11-30` or empty, done. |
| `meta` | Single values of the database itself: its folder, counters for maintenance, the columns chosen for each view (`columns:<view key>`), and the filters of each saved search (`search-filters:<id>`: JSON with the `filters` of the editor and the `query` they come to). They are kept here so that the table of searches stays the one older versions know; filters whose `query` is no longer the search's are not used (`Library::savedSearchFilters()`). |

### documents

| Column | Meaning |
| --- | --- |
| `source_id` | The location it belongs to; 0 for documents created in Indeks, for those kept when their location was removed, and for files that were added one by one before Indeks stopped taking single files. |
| `url` | Where the file is. Unique. Messages of a mailbox have `indeks-part:<mailbox>/<id>`. |
| `mime`, `kind` | MIME type and the kind derived from it (`Kind` in `types.h`). |
| `size`, `mtime`, `hash` | Of the file as it was read. A scan compares size and time; the hash recognises moved files and duplicates. `mtime = 0` marks a file to be read again. |
| `added` | When the document came into the index. |
| `local_copy` | Path of the working copy, if there is one. |
| `owned` | Created in Indeks; the file is in `files/`. |
| `inbox`, `flagged`, `comment` | What the user set. `flagged` is what the interface calls pinned. |
| `trashed` | 0 no; 1 in the trash; 2 emptied from the trash while its file still exists, kept hidden so that a scan does not bring it back. |
| `missing` | 0 no; 1 the file is gone; 2 the file is gone and the document was put into the trash for it, from where it returns by itself when the file does. |
| `leftover` | Stayed behind when its location was removed, for what is attached to it. |
| `pages`, `ocr` | Number of pages; state of text recognition (`OcrState`). |
| `parent_id`, `part_offset`, `part_length` | For a message of a mailbox: the mailbox document and where the message stands in the file. |

## Changing the schema

1. Raise `SchemaVersion` in `database.h`.
2. For a new table or index: add it to a step `if (current < N)` in `Database::migrate()`.
   For a new column of an existing table: add it to the `CREATE TABLE` in `schema`, and add
   an `ALTER TABLE` step guarded by `current >= 1 && current < N` for existing databases.
3. Say in a comment on the step what the version brings and why.
4. Add what the tests need; `librarytest` always starts from a new database, so an upgrade
   of real data is worth trying by hand on a copy.

## Rules of conduct

- Use `Database::exec()` and `Database::value()`, which go to the connection of the calling
  thread. Never pass a `QSqlQuery` between threads.
- Group writes in a `Transaction`, and keep it short: the other threads wait meanwhile.
- A removed document takes its rows in the dependent tables with it (`ON DELETE CASCADE`),
  except to-dos, which outlive it: see `Store::removeDocument()`.
- What the user attached keeps a document whose file or location is gone
  (`Store::hasUserData()`). A new table of things the user attaches belongs in that
  function, for the document and for its parts, as `document_links` is. A document that is
  emptied from the trash but stays as a hidden row (`trashed = 2`) loses its links there
  and then, in `Library::emptyTrash()`, since the row does not go and nothing cascades.
