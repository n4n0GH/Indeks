<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Databases

A *database* is a folder in which Indeks keeps everything it knows: the index, the documents
you created, the copies it made for editing and drawing, and your scripts. Indeks starts with
one database, the *local database*. You can add more, for example one on a network share.

## What a database holds

| In the folder | Content |
| --- | --- |
| `index.sqlite` | The index: documents, their text, tags, groups, notes, drawings, to-dos, rules, saved searches, and the columns chosen for each view |
| `files/` | Documents created in Indeks |
| `copies/` | Working copies of documents that were edited or drawn on |
| `scripts/` | Scripts |

The local database is in `~/.local/share/indeks`.

The files of indexed locations are **not** in the database. The index only records where they
are.

## Working with databases

One database is open at a time. The button at the bottom of the sidebar says which.

- **Switching:** Click the button and choose another database.
- **Adding:** *Add → Add Database…* opens the database in a folder you choose, or starts a new
  one there.
- **Renaming:** *Rename Database…* in the menu, the pencil in the list of databases, or a
  right-click on the button. The name is this computer's word for the database; the folder
  keeps its own name.
- **Managing:** *Databases…* lists all databases and lets you remove one from the list. The
  folder is not deleted.

## Databases on network shares

A database can live in a folder on a mounted share, for example on a NAS.

- **One computer at a time.** A note in the folder keeps a second computer from opening the
  same database while it is in use.
- **When the share is away.** A database whose folder cannot be reached is listed as *Not
  connected*. If it is the one to open at start, or if it goes away while open, Indeks opens
  the local database and says so.
- **Moved folders.** If the share is mounted under another path the next time, the database
  still finds its own documents and copies.
- **Addresses of files.** The index records the addresses of indexed files as this computer
  sees them. A database used from several computers works best for locations that all of them
  reach under the same address: shares mounted at the same path, or `sftp://`, `smb://` and
  similar addresses.
- **Scripts.** See *Automation → Scripts in shared databases*.

## Where Indeks keeps things

| Path | Content |
| --- | --- |
| `~/.local/share/indeks/` | The local database |
| `~/.cache/indeks/` | Downloads of remote documents, PDFs made of Office documents, messages taken out of mailboxes. Safe to delete. |
| `~/.config/indeksrc` | Settings, including the list of databases |
| `~/.config/indeks-allowed-scripts` | Which scripts of shared databases may run on this computer |

## Space

The index does not only grow. When documents have been removed, Indeks gives the freed space
back to the file system. This happens in the background after a location was removed or the
trash was emptied, and at start if a good part of the file is unused.
