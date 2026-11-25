<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Databases

A *database* is a folder in which Indeks keeps everything it knows: the index, the documents
you created, the copies it made for editing and drawing, and your scripts. Every installation
has the *local database*, in a folder of this computer. You can add more, for example one on
a network share.

## The first start

A new installation asks which database to begin with, before it makes anything:

- **Start a new database** in the usual folder, which is the local database, or in a folder
  you choose. What is said under *Starting a new database* below goes here too.
- **Open an existing database** by its file `index.sqlite`, as under *Adding a database*.

If you begin with a database elsewhere, the local database is still in the list. It is
empty, and is only made when it is first opened: by you, or by Indeks when the other one
cannot be reached.

## What a database holds

| In the folder | Content |
| --- | --- |
| `index.sqlite` | The index: documents, their text, tags, groups, links, notes, drawings, to-dos, rules, saved searches, and the columns chosen for each view |
| `files/` | Documents created in Indeks |
| `copies/` | Working copies of documents that were edited or drawn on |
| `scripts/` | Scripts |

The local database is in `~/.local/share/indeks`.

`index.sqlite` is what makes a folder a database. Indeks marks the file as its own. A file
of that name that belongs to another program is not opened and not changed.

The files of indexed locations are **not** in the database. The index only records where they
are.

## Working with databases

One database is open at a time. The button at the bottom of the sidebar says which.

- **Switching:** Click the button and choose another database. While its documents and the
  numbers in the sidebar are being read, the activity display says *Opening database…*.
- **Adding:** *Add → Add Database…* opens a database that exists. See *Adding a database*
  below.
- **Starting a new one:** *Add → New Database…*. See *Starting a new database* below.
- **Renaming:** *Rename Database…* in the menu, the pencil in the list of databases, or a
  right-click on the button. The name is this computer's word for the database; the folder
  keeps its own name.
- **Managing:** *Databases…* lists all databases and lets you remove one from the list. The
  folder is not deleted.

## Adding a database

*Add Database…* is for a database that is there already, on a share or an external drive for
instance.

1. Choose *Add → Add Database…*.
2. Choose the file `index.sqlite` of the database, with *Choose…* or by typing its path. The
   file dialog leaves out files that have nothing to do with a database, so a folder that
   holds one is told from one that does not at a glance.
3. Give the database a name if you like, and choose *Open Database*.

You choose the file and not the folder because the file is the proof that a database is
there. Indeks does not start a database here: if what you chose is not the index of an Indeks
database, the dialog says so and nothing is opened.

- **A file with another name**, such as a copy called `backup.sqlite`, is not taken. A
  database is a folder with a file called `index.sqlite` in it.
- **Several SQLite files in one folder** are no trouble: only `index.sqlite` is the index,
  and Indeks does not touch the others.

## Starting a new database

1. Choose *Add → New Database…*.
2. Choose the folder for it, and a name if you like.
3. Choose *Create Database*.

Give a database a folder of its own. If the folder you choose is not empty, the dialog warns
that the database would put its files among what is there, and the button reads *Create
Database Here Anyway*. A folder that holds an Indeks database already is turned down; add
that database instead.

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

Installed with the system, or run as an AppImage:

| Path | Content |
| --- | --- |
| `~/.local/share/indeks/` | The local database |
| `~/.cache/indeks/` | Downloads of remote documents, PDFs made of Office documents, messages taken out of mailboxes. Safe to delete. |
| `~/.config/indeksrc` | Settings, including the list of databases |
| `~/.config/indeks-allowed-scripts` | Which scripts of shared databases may run on this computer |
| `~/.local/state/indeksstaterc` | The size and place of the window, and the folders last used in file dialogs |

The Flatpak keeps all of this in one folder of its own,
`~/.var/app/io.github.n4n0gh.indeks/`:

| Path in that folder | Content |
| --- | --- |
| `data/indeks/` | The local database |
| `cache/indeks/` | The cache. Safe to delete. |
| `config/indeksrc` | Settings, including the list of databases |
| `config/indeks-allowed-scripts` | Which scripts of shared databases may run on this computer |

Databases you started or added somewhere else are in the folders you chose for them, with
either kind of installation. *Databases…* lists where each one is.

## Uninstalling Indeks

Uninstalling removes the program and nothing else. Your databases and settings stay, so that
nothing of yours is lost by it, and Indeks finds everything again if you install it anew. To
clean up as well:

- **Flatpak:** In a terminal, `flatpak uninstall --delete-data io.github.n4n0gh.indeks`
  removes Indeks together with its folder. If Indeks is uninstalled already, Discover says on
  its page that it still has user data and settings present, and offers *Move to Trash*; or
  delete the folder `~/.var/app/io.github.n4n0gh.indeks` yourself.
- **Installed with the system, or AppImage:** Delete the paths of the first table above,
  and the AppImage file.
- **Databases in other folders:** No uninstalling touches them. Look up where they are under
  *Databases…* before you uninstall, and delete the folder of each database you no longer
  want.

> **Note:** A database holds the documents you created in Indeks, in its folder `files`, and
> your tags, notes and drawings. Copy out what you want to keep before you delete it. The
> files of indexed locations are not in the database; they stay where they are.

## Space

The index does not only grow. When documents have been removed, Indeks gives the freed space
back to the file system. This happens in the background after a location was removed or the
trash was emptied, and at start if a good part of the file is unused.
