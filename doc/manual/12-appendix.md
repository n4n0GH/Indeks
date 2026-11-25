<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Appendix

Reference tables: the terms used in this manual, keyboard shortcuts, all commands, the search
language, and what scripts can do.

## Glossary

- **Command bar:** The list of all commands, opened with `Ctrl+Alt+I`.
- **Comment:** A free text about a document as a whole.
- **Database:** The folder in which Indeks keeps its index, the documents created in it, its
  working copies and scripts.
- **Document:** What stands for a file in Indeks, together with everything you attached.
- **Filter bar:** The bar above the list in which a search is put together by picking.
- **Group:** A collection of documents. A document can be in several groups.
- **Inbox:** The place for documents that still need your attention.
- **Index:** What Indeks knows about your files: where they are, their text, and what you
  attached to them.
- **Indexed location:** A folder, on this computer or on a server, whose files Indeks knows.
- **Link:** A connection between two documents that belong together. It shows on both.
- **Mailbox:** A file that holds many e-mail messages (mbox). It opens like a folder.
- **Note:** A text attached to a document. A document can have many.
- **Pin:** A mark on a document that lists it under *Pinned*.
- **Preview:** The part of the window that shows the selected document.
- **Rule:** A set of filters and actions that files documents automatically.
- **Saved search:** A search kept in the sidebar under a name.
- **Script:** A small Python program that says what should happen to documents.
- **Side panel:** The panel on the right, at first with the *Information*, *Find* and
  *To-Do* tabs.
- **Smart group:** One of the searches that come with Indeks, such as *Added Today*.
- **Sticky note:** A note stuck to a place on a page or picture.
- **Tag:** A word attached to documents.
- **To-do:** A line in the to-do list, with an optional deadline and linked document.
- **Website:** A document that holds the address of a page on the web and opens it in the
  browser.
- **View:** What the list shows for one entry of the sidebar. Each view has its own columns.
- **Working copy:** The copy Indeks makes of a file before you edit it or draw on it.

## Keyboard shortcuts

All shortcuts can be changed under *Configure Keyboard Shortcuts…*.

### Getting around

| Keys | Command |
| --- | --- |
| `Ctrl+Alt+I` | Open the command bar |
| `Ctrl+F` | Go to the search field |
| `Ctrl+Alt+F` | Show or hide the filter bar |
| `Ctrl+1` | Go to Inbox |
| `Ctrl+2` | Go to All Documents |
| `Ctrl+3` | Go to Pinned |
| `Ctrl+4` | Go to Trash |
| `Ctrl+5` | Go to Rules |
| `Ctrl+6` | Go to Scripts |
| `Ctrl+B`, `F9` | Show or hide the sidebar |
| `F10` | Show or hide the preview |
| `Ctrl+Shift+B`, `F11` | Show or hide the side panel |
| `F5` | Update the index |

### Adding and creating

| Keys | Command |
| --- | --- |
| `Ctrl+O` | Add Indexed Folder… |
| `Ctrl+N` | New Plain Text Document… |
| `Ctrl+Shift+V` | New Document from Clipboard… |
| `Ctrl+Shift+G` | New Group… |
| `Ctrl+Shift+S` | Save Search… |

### Working with documents

| Keys | Command |
| --- | --- |
| `Ctrl+Return` | Open File |
| `Ctrl+T` | Add Tag… |
| `Ctrl+G` | Add to Group… |
| `Ctrl+Shift+F` | Pin or unpin |
| `Ctrl+Shift+M` | Add Note… |
| `Ctrl+L` | Manage Linked Documents… |
| `Ctrl+E` | Edit text, or draw on the document |
| `F2` | Rename… (documents created in Indeks) |
| `Ctrl+Alt+C` | Copy Location |
| `Delete` | Move to Trash; under *Rules* and *Scripts*, delete the selected rule or script |
| `Ctrl+A` | Select all documents; in a text or on PDF pages, select the text |
| `Ctrl+C` | Copy the selected text or table cell |

### In the list

| Keys | Command |
| --- | --- |
| `Up`, `Down` | Move the selection |
| `Shift+Up`, `Shift+Down` | Extend the selection |
| `Page Up`, `Page Down`, `Home`, `End` | Move further |
| `Right`, `Left` | Open and close a mailbox |
| `Left`, `Right`, `Up`, `Down` | Among thumbnails: move the selection to the next tile that way |
| `Ctrl` with the mouse wheel | Among thumbnails: make them larger or smaller |

### In the preview

| Keys | Command |
| --- | --- |
| `Ctrl++`, `Ctrl+-` | Zoom in and out |
| `Ctrl+0` | Reset the zoom |
| `Ctrl` + mouse wheel | Zoom |
| `Ctrl+U` | Show unformatted, or view raw |
| `Ctrl+D` | Find in the document |
| `F3`, `Shift+F3` | To the next and the previous place found in the document |
| `Ctrl+Enter` | Put away a sticky note or add a note that is being written |

## Commands

Every command is in the command bar. This list says what each one does.

### Adding and creating

- **Add Indexed Folder…:** Makes a folder an indexed location.
- **Add Remote Location…:** Makes a folder on a server an indexed location.
- **New Plain Text Document…**, **New Markdown Document…**, **New Formatted Document…:**
  Create a document stored in Indeks.
- **New Document from Clipboard…:** Makes a document stored in Indeks of the text, the
  picture or the address of a website on the clipboard.
- **New Group…**, **New Tag…**, **New Rule…**, **New To-Do…**, **New Script…:** Create one.
- **Update Index:** Looks through all locations for changes.

### Documents

- **Open File**, **Open With…:** Open the document in another application. A website opens
  in the browser.
- **Show in File Manager:** Shows the file in its folder.
- **Copy Location:** Copies the path or address of the document.
- **Copy Web Address:** Copies the address a website holds.
- **Rename…:** Renames a document created in Indeks.
- **Add Tag…**, **Add to Group…**, **Pin**, **Add Note…:** See *Organising*.
- **Create To-Do for Document…:** Creates a to-do linked to the document.
- **Manage Linked Documents…:** Shows the documents the selected one is linked to, removes
  links and finds documents to link.
- **Link Selected Documents:** Links every one of the selected documents with every other.
- **Find Similar-Looking Documents**, **Find Roughly Similar-Looking Documents:** For a
  picture or a PDF: narrows the list to the documents that look like it, by their
  letterhead or as a picture. See *Finding → Documents that look alike*.
- **Edit:** Edits the text of the document, or draws on it.
- **Remove All Drawings:** Removes the drawings from the document.
- **Remove from Inbox**, **Move to Inbox:** Take a document out of the inbox, or put it back.
- **Remove from This Group:** Takes the document out of the group selected in the sidebar.
- **Recognize Text (OCR):** Runs text recognition on the document again.
- **Read Again:** Reads the document for the index again.
- **Discard Copy and Return to Original:** Drops the working copy of the document.
- **Move to Trash**, **Restore from Trash**, **Empty Trash…:** See *In and Out → The trash*.
- **Select All Documents:** Selects everything in the list.
- **Run Script “name”:** Runs that script on the selected documents.

### Sidebar

- **Go to Inbox**, **Go to All Documents**, **Go to Pinned**, **Go to Trash:** Select that
  entry.
- **Edit Selected Sidebar Entry…**, **Delete Selected Sidebar Entry…:** Change or remove the
  selected group, tag, saved search or location.
- **Put New Files of the Selected Location into the Inbox:** Switches that for the location.
- **Save Search…:** Keeps the current search in the sidebar.

### View

- **Search:** Goes to the search field.
- **Advanced Search:** Shows or hides the filter bar.
- **Show Sidebar**, **Show Preview**, **Show Side Panel:** Show or hide that part.
- **No Preview**, **Preview Below Documents**, **Preview Beside Documents:** The three
  choices of the *Preview* button in the toolbar.
- **Show Documents as List**, **Show Documents as Thumbnails:** Rows with columns, or small
  pictures in a grid. See *The Window → Thumbnails*.
- **Larger Thumbnails**, **Smaller Thumbnails:** The size of the thumbnails, a step at a
  time.
- **Sort Documents by Name** (and **by Type**, **Date Modified**, **Date Added**, **Size**,
  **Sender**, **Location**, **Number of Pages**, **Comment**), **Sort Documents in
  Descending Order:** What the *Sort By* menu above the documents offers.
- **Hide the Tab Bar Where There Is Only One View:** Leaves out the row of tabs of an area
  of the sidebar or the side panel that has a single tab. See *The Window → Arranging the
  panels*.
- **Sort Table by Current Column, A to Z** (and **Z to A**, **Smallest Number First**,
  **Largest Number First**), **Show Table in the Order of the File**, **Hide Current Column
  of the Table**, **Show All Columns of the Table:** For a table in the preview, what the
  menu of its columns offers, for the column of the cell that is chosen. See
  *Documents → Tables*.
- **Move Section of the Selected Sidebar Entry Up**, **… Down:** Changes the order of the
  sections of the *Database* tab, like dragging a section by its title.
- **Preview Beside the List:** Puts the preview to the right of the list instead of below
  it, or back.
- **Show Document Information**, **Show To-Do List:** Bring that tab up, in whichever
  panel it is, and show the panel if it was hidden.
- **Move … to the Left Panel**, **Move … to the Right Panel:** Puts a tab of the sidebar or
  the side panel into the other one. There is one such command for each of *Database*,
  *Automation*, *Information*, *To-Do* and *Find*. See *The Window → Arranging the panels*.
- **Split … Below in Its Panel**, **Merge … to the Top of Its Panel:** Gives a tab the lower
  area of its panel, or puts it back among the tabs of the upper one.
- **Move the … Tab One Place to the Left**, **… to the Right:** Changes the order of the
  tabs of an area.
- **Reset Sidebar and Side Panel:** Puts every tab back where it was at first.
- **Find in Document…:** Opens the *Find* tab of the side panel, for searching within the
  document that is shown. See *Finding → Finding inside a document*.
- **Find Next in Document**, **Find Previous in Document:** Go to the next or the previous
  place the text was found at.
- **Choose List Columns and Row Height…:** Opens the column menu.
- **Zoom In**, **Zoom Out**, **Reset Zoom:** Change the size of the preview.
- **Show Unformatted:** Shows the document as it is in the file, where the preview lays
  it out for reading: Markdown, HTML, tables, JSON, and plain text files that turned out to
  be one of these. Called *View Raw* for tables and data.
- **Show Indexing Activity:** Lists what Indeks is working on.

### Scripts, to-dos and databases

- **Go to Rules**, **Go to Scripts:** Show the rules or the scripts in the middle of the
  window.
- **Apply Selected Rule:** Runs the rule that is selected in the list of rules on everything
  it matches.
- **Delete Rule…**, **Move Script to Trash…:** Remove the rule or script that is selected in
  its list. The command is named after what is selected.
- **Run Script…:** Lists the scripts to pick one to run on the selected documents.
- **Show Script Output**, **Stop Running Scripts:** See *Automation*.
- **Remove Completed To-Dos:** Removes the to-dos that are done.
- **Databases…**, **Rename Database…:** See *Databases*.
- **Add Database…:** Opens a database that exists, chosen by its file `index.sqlite`.
- **New Database…:** Starts an empty database in a folder.

### Application

- **Indeks Manual:** Opens this manual.
- **Configure Indeks…:** Opens the settings.
- **Configure Toolbar…**, **Configure Keyboard Shortcuts…:** Change the toolbar and shortcuts.
- **About Indeks**, **Quit**.

## Search language

Words match names, text and notes. A word also matches the beginning of a longer word. Put a
phrase in quotes. A minus in front excludes a word or term. `OR` between two matches either.

| Term | Meaning |
| --- | --- |
| `tag:name` | Has the tag. `tag:none`: has no tag. |
| `group:name` | Is in the group. `group:none`: is in no group. |
| `kind:` | `text`, `rich`, `pdf`, `image`, `email`, `canvas`, `website` |
| `ext:` | File name extension, without the dot |
| `mime:` | Part of the MIME type, such as `mime:pdf` |
| `name:` | A word in the name; with `*` or `?`, a pattern for the whole name |
| `path:` | Part of the path of the folder the file is in |
| `source:` | The name of an indexed location |
| `content:` | A word or phrase in the text of the document only |
| `note:` | A word or phrase in notes and comments only |
| `added:`, `modified:` | `today`; a span such as `7d` or `12h`; a day `2026-01-31`; a month `2026-03`; with `>` or `<`, after or before |
| `size:` | With `>` or `<` and a number with `k`, `M` or `G` |
| `is:inbox` | Is in the inbox |
| `is:pinned` | Is pinned |
| `is:annotated` | Has a comment, notes or drawings |
| `is:linked` | Is linked to another document |
| `is:unfiled`, `is:untagged` | Is in no group; has no tag |
| `is:duplicate` | Has the same content as another file |
| `is:missing` | Its file is gone |
| `is:local` | Is stored in Indeks |
| `is:copied` | Has a working copy |
| `is:ocr` | Its text was recognised |
| `is:trashed` | Is in the trash |
| `partof:number` | The messages of the mailbox with that number |
| `similar:number` | Looks like the document with that number; `similar:~number`: roughly like it. The filter bar writes these; see *Finding → Documents that look alike*. |

## Script reference

A script is a Python file with a function `process(document)`. Indeks calls it once for every
document the script is run on.

### Reading a document

| Name | Value |
| --- | --- |
| `document.name` | The file name |
| `document.extension` | Its ending in small letters, without the dot |
| `document.text` | The text Indeks found, recognised text of scans included |
| `document.path` | The file to read; `None` while it is not on this computer |
| `document.url` | Where the original is |
| `document.kind` | `"text"`, `"rich"`, `"pdf"`, `"image"`, `"email"`, `"canvas"`, `"website"` or `"other"` |
| `document.mime_type` | For example `"application/pdf"` |
| `document.size` | In bytes |
| `document.pages` | The number of pages of a PDF |
| `document.modified`, `document.added` | A `datetime` |
| `document.location` | The name of the indexed location it is in |
| `document.tags` | A list of names; also `document.has_tag("name")` |
| `document.groups` | A list of tuples such as `("Finance", "Invoices")`; also `document.in_group("Finance", "Invoices")` |
| `document.pinned`, `document.inbox` | `True` or `False` |
| `document.comment`, `document.notes` | The comment; a list of the notes |

### Asking for changes

| Call | Effect |
| --- | --- |
| `document.add_tag("name")` | Adds a tag |
| `document.remove_tag("name")` | Removes a tag |
| `document.add_to_group("Finance", "Invoices")` | Adds to a group; groups that are missing are created |
| `document.remove_from_group("Finance", "Invoices")` | Removes from a group |
| `document.pin()`, `document.unpin()` | Pins or unpins |
| `document.remove_from_inbox()`, `document.move_to_inbox()` | Takes out of, or puts into, the inbox |
| `document.comment = "…"` | Sets the comment |
| `document.add_note("…")` | Adds a note; the same note is not added twice |

What `print()` writes is kept in the script output. If the script raises an error on a
document, nothing is done to that document.

## What Indeks needs

- KDE Plasma with Qt 6.8 and KDE Frameworks 6.8 or later.
- **Tesseract**, with data for your languages, to recognise text in pictures and scans.
- **LibreOffice** to show Office documents as they look and to read the old Office formats.
- **Python 3** and **bubblewrap** to run scripts. The Flatpak brings its own Python and needs
  no bubblewrap.

Indeks works without the last three; the features that need them say what is missing.
