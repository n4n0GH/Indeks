<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Settings

*Configure Indeks…* in the menu opens the settings. They take effect at once.

## Indexed Locations

Lists the folders and servers Indeks indexes. Each entry shows the name, the address, how
many documents it has and when it was last checked.

- **Inbox button:** Switches whether files that turn up in this location later on are put
  into the inbox. The line under the address says when this is on.
- **Update Now:** Looks through the location for changes.
- **Stop Indexing:** Removes the location from Indeks. The files are not touched.
- **Add Folder…**, **Add Remote Location…:** Add a location.

## Indexing

**What to index**

- **Index the content of files:** Makes the text inside documents searchable. Without this,
  documents are found by name, tags and notes only.
- **Include hidden files and folders:** Also indexes files and folders whose name starts with
  a dot. Switching this on reads all locations again; the hidden files found are not put into
  the inbox.
- **Read old Office documents with LibreOffice:** Word, Excel and PowerPoint files in the
  formats from before 2007 cannot be read otherwise. This takes about a second per document.
- **Files read at the same time:** How many helper programs read files side by side.
  *Automatic* uses half of the processor cores, eight at most.
- **Read content of files up to (MB):** Larger files are indexed by name only.
- **Download remote files up to (MB):** Larger files on servers are indexed by name only.

**When to index**

- **Look for changes when Indeks starts:** Goes through all locations at start. Folders on
  this computer are also watched while Indeks runs.
- **Update Index Now:** Does it right away.

## Text Recognition

Indeks uses the program Tesseract to recognise text in pictures and scanned PDFs. If Tesseract
is not installed, this page says so.

- **Recognize text in images and scanned PDF documents:** Runs in the background after
  indexing. The recognised text is stored in the index; the files stay as they are.
- **Pages read at the same time:** Each uses one processor core at low priority. *Automatic*
  uses three quarters of the cores.
- **Languages to look for:** The languages Tesseract has data for. Choosing fewer languages
  makes recognition faster and more accurate. More languages are installed as packages of
  your distribution.

*Recognize Text (OCR)* in the context menu of a document runs recognition on it again.

## To-Do List

- **Highlight to-dos that are due within:** How many days before its deadline a to-do is
  highlighted in orange, from *On the day only* up to 365 days. To-dos past their deadline
  are always highlighted in red.

## Toolbar

- **Buttons, from left to right:** The buttons between the sidebar button and the search
  field. Use the arrows to move a button and the minus to take it off.
- **Add Button…**, **Add Separator:** Add a command or a dividing line.
- **Restore Defaults:** Returns to the buttons Indeks starts with.
- **Show text beside the icons:** Without text more buttons fit; their names appear when the
  pointer rests on them.

The buttons for the panels, the search field and the menu keep their places. *Edit* and *Add
Note…* have their place in the bar above the preview and are not offered here.

## About Indeks

Shows the version of Indeks and of the libraries it uses.

## Keyboard shortcuts

*Configure Keyboard Shortcuts…* in the menu lets you change the shortcut of every command,
including the commands that run a script.
