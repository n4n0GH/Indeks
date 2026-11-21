<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# The database

A database is a folder. The index in it is one SQLite file, `index.sqlite`; beside it are
`files/` (documents created in Indeks), `copies/` (working copies) and `scripts/`.

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
| `searches` | Saved searches: name and query. |
| `rules` | Rules: name, the compiled query, the filters and actions as JSON, whether active and whether run on import. |
| `annotations` | Notes (`kind = 'note'`) and drawings (`kind = 'ink'`), per document and page. `x`, `y`: where a sticky note stands, as shares of the page; -1 for none. |
| `todos` | The to-do list: text, linked document (0 for none), deadline as `2026-11-30` or empty, done. |
| `meta` | Single values of the database itself: its folder, counters for maintenance, and the columns chosen for each view (`columns:<view key>`). |

### documents

| Column | Meaning |
| --- | --- |
| `source_id` | The location it belongs to; 0 for documents added by hand or created in Indeks. |
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
