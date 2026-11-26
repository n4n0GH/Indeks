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
| `to-dos.ics` | The to-dos that have a deadline, as a calendar, if that is turned on. See *Organising → To-dos in the calendar* |

The local database is in `~/.local/share/indeks`, or in
`~/.var/app/io.github.n4n0gh.indeks/data/indeks` if you installed Indeks as a Flatpak. See
*Where Indeks keeps things* below.

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
| `~/.cache/indeks/` | Downloads of remote documents, PDFs made of Office documents, messages taken out of mailboxes, thumbnails. Safe to delete. |
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
| `.local/state/indeksstaterc` | The size and place of the window, and the folders last used in file dialogs |

Databases you started or added somewhere else are in the folders you chose for them, with
either kind of installation. *Databases…* lists where each one is, the local one included.

All of these are hidden: `.local`, `.cache`, `.config` and `.var` begin with a dot, and file
managers leave such folders out. In Dolphin, `Ctrl+H` shows them; or type the path into the
bar at the top of the window.

## Backing up

Indeks has no backup of its own, and needs none: a database is a folder, and every backup
program copies folders. Use the one you use for the rest of your files.

**What to back up.**

- **The folder of each database.** It holds what exists nowhere else: your tags, groups,
  links, notes, drawings, to-dos, rules and saved searches, the documents you created in
  Indeks and the copies you edited. Where it is: see the tables above, or *Databases…*.
  Mind the folder of the Flatpak, which a backup of `~/.local` alone does not reach.
- **Your documents.** The files of indexed locations are not in the database. Back up their
  folders too; without the files, a database that is brought back knows the names, the text
  and your notes, but has nothing to show.
- **The settings,** if you like: the file `indeksrc` holds the list of your databases and how
  you arranged the window. The cache needs no backup.

**When.** While Indeks is open, the index is being written to: beside `index.sqlite` stand
helper files whose names begin with `index.sqlite-`, and they belong together. A copy that
is taken while they change can be an index that does not open. So:

- let the backup run while Indeks is closed, or
- back up from a snapshot of the file system (Btrfs, ZFS, LVM), which holds all files as
  they were at one moment.

**With what.** Some programs that are common on Linux. Indeks asks nothing special of any of
them.

- **Kup:** The backup of KDE Plasma, set up in the System Settings. It copies folders to an
  external drive or a share, with or without older versions.
- **Déjà Dup** (*Backups* in GNOME) and **Pika Backup:** Simple programs that back up on a
  schedule.
- **Vorta:** A window for BorgBackup. **BorgBackup**, **restic** and **Kopia** themselves
  are used in a terminal. They keep many versions in little room, encrypted, on drives,
  servers and storage on the internet.
- **Back In Time**, **rsnapshot** and **rsync:** Plain copies of folders, with older
  versions or without.
- **Snapper** and **btrbk:** Snapshots on Btrfs. A snapshot on the same disk guards against
  a mistake, not against losing the disk; btrbk also sends snapshots to another disk.

Two lines for a terminal, as examples: a plain copy with rsync, and a backup of the database
together with a folder of documents with restic, into a store that `restic init` has made
before.

```sh
rsync -a ~/.local/share/indeks/ /run/media/you/Backup/indeks/
restic -r /run/media/you/Backup/restic backup ~/.local/share/indeks ~/Documents
```

> **Note:** Programs that keep folders the same on several computers, such as Syncthing,
> Nextcloud or Dropbox, are no backup: what is deleted or damaged here is deleted or damaged
> there a moment later. And a database in such a folder must never be open on two computers
> at once.

**Getting it back.**

1. Quit Indeks.
2. Put your documents back where they were, under the same paths.
3. Put the folder of the database back where it was. Indeks opens it at the next start.

- **On a new computer,** put the local database into its usual folder before you start
  Indeks for the first time, and Indeks opens it without asking. Or put it anywhere, and
  choose *Open an existing database* at the first start.
- **In another folder than before:** *Add → Add Database…*, and choose its `index.sqlite`.
- **Documents that are not where they were** are taken for deleted and go to the trash in
  Indeks, with everything attached to them. They leave it by themselves once the files are
  back in their places. See *In and Out → When a file goes away*.

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
