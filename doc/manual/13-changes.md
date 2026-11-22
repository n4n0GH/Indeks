<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Changes

What is new in Indeks, the latest first. This manual is changed together with Indeks, so the
chapters before this one always describe the version named on the cover.

## Version 0.3.0

- **Linked documents:** Documents can be linked to one another, for papers that belong
  together although they are separate files. *Manage Linked Documents…* (`Ctrl+L`), in the
  context menu and in the *Information* tab, finds documents to link. *Link Selected
  Documents* links several at once. The *Information* tab lists the links of a document and
  leads from one to the other, and `is:linked` finds the documents that have any. See
  *Organising → Linked documents*.
- **Databases are brought up to date:** This version adds the links to the index the first
  time it opens a database. Version 0.2.0 and older cannot open that database afterwards.
  If you use a database from several computers, update Indeks on all of them.
- **Rules for what is not there yet:** In the dialog of a rule, filter choices that no
  document matches at the moment can be chosen. Before, a rule could not be kept to the
  inbox while the inbox was empty, or be given a tag that no document had. See *Automation →
  A rule for the inbox only*.

## Version 0.2.0

- **First start:** A new installation asks whether to start a new database, in the usual
  folder or one you choose, or to open one that exists.
- **Adding and starting databases:** *Add Database…* now opens a database by its file
  `index.sqlite` and never starts one. Starting one is *New Database…*, which warns when the
  folder is not empty. Indeks marks its index files as its own and leaves a file of that name
  that belongs to another program untouched.
- **Folders, not single files:** Files come into Indeks through indexed locations only.
  *Add Files…* is gone, a file dropped onto the window or named when Indeks is started is no
  longer added, and Dolphin offers *Send to Indeks* for folders only. Documents that were
  added as single files before stay. See *In and Out → Folders, not single files*.
  *Add Indexed Folder…* is now on `Ctrl+O`.

## Version 0.1.0

The first release. What it brings, the latest additions first:

- **Flatpak and AppImage:** Releases are built in both forms, and both update themselves.
- **Manual:** This manual. *Indeks Manual* (`F1`) opens it. Its table of contents can be
  clicked, and it has bookmarks for chapters and sections.
- **Columns per view:** Every view of the list has its own columns.
- **Notes in the preview bar:** *Edit* and *Add Note…* moved from the toolbar to the bar
  above the preview. On pages, *Add Note* offers *Add Note…* and *Add Sticky*.
- **Text selection on pages:** Dragging selects text on PDFs and Office documents from
  anywhere on the page; a double click selects a word.
- **To-do list:** The side panel has tabs for the document information and a to-do list with
  deadlines and linked documents.
- **Trash for vanished files:** A document whose file was deleted goes to the trash instead
  of being forgotten, and returns when the file does.
- **Documents stay in view:** A document you work on stays in the list until you move on.
- **Inbox:** New files in indexed locations are put into the inbox. Tagging, grouping,
  pinning or noting a document takes it out.
- **Pinning:** What was called flagging is now pinning. `is:flagged` still works in searches.
- **Mailboxes:** Mailbox files open like folders, and their messages are documents.
- **Scripts:** Python scripts in a sandbox, run by hand or by rules.
- **Rules:** Filters and actions are lists; actions can be reordered.
- **Preview:** One bar above the preview for every kind of document, zoom everywhere, tables
  for CSV and TSV files, and *Show Unformatted* for Markdown and HTML.
- **Sticky notes:** Notes can be stuck to a place on a page or picture.
- **Databases:** Databases can be renamed; the database button is at the bottom of the
  sidebar.
