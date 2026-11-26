<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Changes

What is new in Indeks, the latest first. This manual is changed together with Indeks, so the
chapters before this one always describe the version named on the cover.

## Since version 0.5.0

- **From thumbnails to the list:** When Indeks was started with the documents shown as
  thumbnails, the first switch to the list made a row for every document that was listed
  instead of for those in view. With many thousands of them, in an opened mailbox for
  instance, the window stood still for minutes and was offered to be closed as not
  responding. The switch now takes no longer than any other.

## Version 0.5.0

- **Documents that look alike:** *Find Similar-Looking Documents* lists the PDFs and
  pictures that look like the selected one: pages with the same letterhead or logo at the
  same place, whatever the files are called and whatever is written below, and pictures
  that are the same picture in another size. It is a filter of the filter bar, so it can be
  combined, turned around (*Does Not Look Like …*), saved as a search and used in a rule.
  After the update, Indeks looks at the pictures and PDFs it already has, in the
  background. See *Finding → Documents that look alike*.
- **Documents as thumbnails:** The documents can be shown as small pictures in a grid
  instead of as a list: pictures as themselves, PDFs as their first page, text files as
  the beginning of their text. The size is set with a slider or with `Ctrl` and the mouse
  wheel. See *The Window → Thumbnails*.
- **A bar above the documents:** It says how many documents are listed and selected, and
  has the *Sort By* menu, the switch between list and thumbnails and the slider for the
  size. The order of the documents can now also be set from the command bar.
- **Rows of tabs:** The rows of tabs of the sidebar and the side panel are as high as the
  bars above the documents and the preview. A row with a single tab can be left out:
  *Settings → Panels*.
- **Tables can be sorted, and columns hidden:** Right-click the name of a column or a cell of
  a table in the preview to sort the rows by that column, by its text or by its numbers,
  and to hide the column. A click on the name of a column sorts too. See *Documents →
  Tables*.
- **The sections of the sidebar can be put in order:** Drag *Smart Groups*, *Saved
  Searches*, *Groups*, *Tags* or *Locations* by its title to where it is to stand.
- **The size of the database** stands beside its name at the bottom of the sidebar.
- **One button for the preview:** The two buttons that showed the preview and put it beside
  the list are one button with a menu: *No Preview*, *Preview Below Documents*, *Preview
  Beside Documents*. In a toolbar you arranged yourself it takes the place of the two.
- **A rule or script is always selected:** Under *Rules* and *Scripts* the first entry is
  selected when the list comes up, and after one was deleted the next one is. Before, a
  list with a single entry in it offered nothing to delete until that entry was clicked.
  `Del` now deletes the selected rule or script wherever in the window the keys go, except
  while you type in a text.
- **The panels can be arranged:** The tabs of the sidebar and the side panel can be moved
  to the other panel, and each panel can show two of them at once, one above the other:
  drag a tab to where its view is to be, which is also how tabs are put in order, or
  right-click it and choose *Move to Right Panel*, *Move to Left Panel*, *Split Below* or
  *Merge to Top*. The line between the two areas can be dragged. The to-do list now has the
  lower area of the side panel to itself, so that it stays in view. See *The Window →
  Arranging the panels*.
- **Keys for the panels:** `Ctrl+B` shows and hides the sidebar and `Ctrl+Shift+B` the side
  panel. `F9` and `F11` do so as before.
- **Text files that are Markdown or tables:** A plain text file (`.txt`) that is written in
  Markdown is shown formatted, and one that holds values in rows and columns is shown as a
  table. That goes for lists with ` | ` between the values too, also where each row has a
  line of text set in below it, which becomes a column of its own. *Show Unformatted* or
  *View Raw* (`Ctrl+U`) shows the text as it is in the file. See *Documents → Text files
  that are something else*.
- **Finding inside a document:** A third tab in the side panel, with the magnifying glass,
  searches within the document that is shown: what you type is marked in the document, the
  places are listed with the words around them, and `Enter`, the arrows or `F3` step through
  them. *Find in Document…* (`Ctrl+D`) opens it. It works in text, tables, PDF and Office
  documents and e-mails. See *Finding → Finding inside a document*.
- **Saved searches are put together like rules:** The dialog for saving and changing a
  search lists its filters one to a line, with *Add Filter*, a pencil to change one, a cross
  to take one off, and the number of documents they find. Searches saved before are taken
  apart into filters where that is possible. See *Finding → Saved searches*.
- **Lists no longer hold the window up:** Every list of documents is read in the
  background. In a large database, going to an entry such as *Duplicates* no longer makes
  the window stand still; the activity display says *Listing documents…* until the list is
  there.
- **Opening a large database:** Indeks no longer stands still while a large database is
  opened. The window comes up at once, the activity display says *Opening database…* until
  the list of documents and the numbers in the sidebar are there, and the window can be used
  meanwhile. This goes for switching to another database too.
- **The numbers in the sidebar are counted in the background:** In a large database every
  change, a tag or a new document, held the window up for a moment while the numbers were
  counted again. They now follow a moment later, without holding anything up.
- **Scripts run in the Flatpak:** The Flatpak could not run scripts, and said that
  bubblewrap was not installed, which no installing would have mended. Scripts now run
  there in a sandbox that Flatpak itself provides, with the same limits: no network,
  nothing of your files but the documents, and those read-only. See *Automation → Scripts*.
- **What to install for scripts:** Where bubblewrap is missing outside the Flatpak, Indeks
  names the package to install.

## Version 0.4.0

- **Rules and scripts have their own tab:** The sidebar has two tabs now, *Database* and
  *Automation*. Under *Automation*, *Rules* and *Scripts* are listed in the middle of the
  window, and the one that is selected is changed where the preview is, instead of in a
  dialog. Rules are no longer a section of the database list; *Show Matching Documents*
  lists what a rule matches. `Ctrl+5` and `Ctrl+6` go there. Indeks asks before changes to
  a rule or script are lost, also when it is closed. See *Automation*.
- **The whole toolbar can be arranged:** The search field, the filter button, *Save Search…*
  and the preview buttons are placed and removed like every other button, and a *Flexible
  Space* says what stands at the right end. In *Settings → Toolbar* entries are dragged to
  where they belong. A toolbar you put together before keeps its buttons and gets these at
  the right, where they were.
- **New Script… in the New menu:** The *New* button of the toolbar offers *New Script…* too.
- **Preview beside the list:** A button beside the preview button in the toolbar, also
  *Menu → Preview → Preview Beside the List*, puts the preview to the right of the list
  instead of below it, for wide screens. See *The Window → Preview*.
- **Websites:** A new kind of document that holds the address of a page on the web. Copy
  the address, choose *New Document from Clipboard…*, and the document opens the page in
  your browser from then on; it takes tags, notes and links like every other. `kind:website`
  finds them. Internet shortcuts (`.url`) and `.webloc` files in indexed locations are
  websites too. See *Documents → Websites*.
- **Documents from the clipboard:** *New → New Document from Clipboard…* (`Ctrl+Shift+V`)
  makes a document of text or a picture that you copied in another program: plain text,
  Markdown, a formatted document or a PNG file, stored in Indeks. See *Documents →
  Documents created in Indeks*.

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
