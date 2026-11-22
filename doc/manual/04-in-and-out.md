<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# In and Out

This chapter is about how documents get into Indeks, how Indeks follows the files they stand
for, and what happens when a file goes away.

## Indexed locations

An *indexed location* is a folder whose files Indeks knows about. The files stay where they
are.

**Adding a folder.** Choose *Add → Add Indexed Folder…* (`Ctrl+O`), drop a folder onto
the window, or use *Send to Indeks* on a folder in Dolphin. Indeks reads the folder and all
folders inside it. Starting Indeks with the names of folders adds them too:

```
indeks ~/Documents
```

**Keeping up.** Folders on this computer are watched while Indeks runs: new, changed and
removed files are noticed within moments. When Indeks starts, it looks through all locations
for changes; this can be switched off under *Settings → Indexing*. *Update Index* (`F5`)
does it on request, and the context menu of a location has *Update Now* for that one.

**Moved and renamed files.** A file that is moved or renamed inside the indexed locations
keeps its document, with all tags and notes.

**External drives.** A folder on an external drive is indexed like any other. While the drive
is unplugged, its documents stay in the index and can be searched; the location is marked as
not connected. When the drive returns, the index catches up.

**Removing a location.** Select it in the sidebar and choose *Stop Indexing This Location…*
from its context menu. Its documents leave Indeks, unless something of yours is attached to
them (see *When a location is removed* below). The files are not touched.

## Remote locations

Indeks can index folders on servers: anything the file manager can open, such as `sftp://`,
`smb://` or `webdav://` addresses.

1. Choose *Add → Add Remote Location…*.
2. Enter the address as the file manager shows it, and a name if you like.

Indeks lists the files and downloads them one at a time to read their text. Nothing is ever
written to the server. Files larger than the limit set under *Settings → Indexing* are
indexed by name only. While the server cannot be reached, its documents stay searchable.
Remote locations are checked for changes from time to time and when you choose *Update Now*.

## Folders, not single files

Indeks takes folders, not single files. A file comes into Indeks through the indexed
location it is in. To have a file in Indeks, put it into a folder that is indexed, or add
the folder it is in.

This is on purpose. A file that is indexed where it happens to lie, among your downloads for
instance, looks like every other file there. Nothing shows that Indeks knows it, and it is
easily tidied away. A folder you made a location is a place you know Indeks looks after.

- **Dropping a file** onto the window adds nothing, and so does starting Indeks with the name
  of a file. Indeks says so.
- **Dolphin** offers *Send to Indeks* for folders only.
- **Documents from before:** Files that were added one by one with an earlier version of
  Indeks stay as they are.

## The inbox and new files

The inbox collects what still needs your attention.

- Documents you create in Indeks start out in the inbox.
- A file that turns up in an indexed location *after* the location was first read is put into
  the inbox as well. What was already there when you added the location is not.
- Rules that run on new documents see a new file first, so a rule can file it and take it
  out of the inbox again.

To switch this off for a location, right-click the location in the sidebar and untick *Put
New Files into the Inbox*, or use the inbox button of the location under *Settings → Indexed
Locations*.

See *Organising → The inbox* for how documents leave it.

## What Indeks reads

Indeks reads the text of plain text, Markdown, HTML, RTF, OpenDocument and Office files, PDF,
e-mails and mailboxes, and Obsidian canvases, and recognises text in pictures and scans. Other
files are indexed by name.

- Files are read by helper programs, several at once. If a file makes a helper fail, that
  file goes into the index by its name and the others are not held up.
- Files larger than the limit under *Settings → Indexing* are indexed by name only.
- Hidden files and folders are left out unless you switch them on in the settings.
- Temporary files, such as unfinished downloads and editor backups, are skipped.

## When a file goes away

Indeks follows the files it indexes, but it does not quietly forget one that is deleted.

When a file is gone from an indexed location, its document goes to the **trash** in Indeks,
with its text, tags and notes, and a message says so. This way a file that was deleted by
accident is noticed. In the trash such a document is struck out and marked with a warning
sign.

- **If the file comes back** to where it was, for example from a backup, the document leaves
  the trash by itself.
- **Restore from Trash** brings such a document back only if something of yours is attached
  to it: a tag, a group, a pin, a link to another document, a comment, a note, a drawing, an
  open to-do, or a text you edited in Indeks. It is then listed as a document whose file is missing, also under
  *Missing Files* in the sidebar.
- **Empty Trash…** is what finally forgets it.

> **Note:** A location that cannot be reached, such as an unplugged drive, is a different
> matter. Nothing is put into the trash; the documents simply wait for the location.

## The trash

*Move to Trash* (`Delete`) puts documents into the trash of Indeks. They no longer show up in
lists or searches. *Restore from Trash* brings them back.

*Empty Trash…* removes the documents in the trash from Indeks.

- Files in indexed locations are **not deleted**. Indeks only stops showing them, also after
  later updates of the index.
- Documents you created in Indeks are moved to the trash of your desktop, from where you can
  still restore them.

## When a location is removed

When you stop indexing a location, its documents go too. Documents with something of yours
attached stay for as long as that is there, and leave when the last of it is removed.

## Getting documents out

Indeks never holds your files captive.

- *Show in File Manager* opens the folder a file is in.
- *Copy Location* (`Ctrl+Alt+C`) copies the path or address of a document.
- Documents can be dragged from the list into other applications.
- *Discard Copy and Return to Original* drops the copy Indeks made for editing or drawing, so
  that the document shows the original file again.
