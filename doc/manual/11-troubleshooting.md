<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Troubleshooting

This chapter collects what to check when something does not behave as expected.

## A document is not found

- **Is it indexed?** Search for its name with `name:`. If nothing comes up, check that its
  folder is listed under *Locations*, and choose *Update Index* (`F5`).
- **Is its text indexed?** Select the document. If the *Information* tab says its content
  could not be read, choose *Read Again* from the context menu. Files larger than the limit
  under *Settings → Indexing* are found by name only.
- **Is it a scan?** The text of scans is recognised after indexing and takes a while. The
  activity display at the bottom of the sidebar shows what is still waiting. Check that
  Tesseract and the right languages are installed under *Settings → Text Recognition*.
- **Is it in the trash?** Searches leave out the trash. Look under *Trash*, or add
  `is:trashed` to the search.
- **Is it hidden?** Files whose name starts with a dot are only indexed when *Include hidden
  files and folders* is switched on.
- **Is it a message in a mailbox?** Messages are listed when a search asks for something of
  theirs, such as a word. A filter for a date or a size alone lists the mailbox instead.

## A document went to the trash by itself

Its file is gone from where it was: deleted, or moved out of the indexed locations. Put the
file back and the document returns. See *In and Out → When a file goes away*.

## A location is shown as not connected

The folder cannot be reached: the drive is unplugged, the share is not mounted, or the server
is down. The documents stay searchable. When the location is back, choose *Update Now* from
its context menu, or wait for Indeks to notice.

## Text recognition does not work

- Tesseract must be installed, together with the data for the languages of your documents.
  *Settings → Text Recognition* says what is missing.
- Very small pictures, such as icons, are not looked at.
- To run recognition on a document again, choose *Recognize Text (OCR)* from its context
  menu.
- **Text on a scan cannot be selected:** The *Information* tab says under *OCR* whether the
  text was recognised. If it says *Waiting*, it is not yet. A scan that an earlier version
  of Indeks recognised is read once more when you first look at it; give it a moment.
- **What is selected or found is not what the page says:** It is what recognition read.
  *Show Recognized Text* (`Ctrl+U`) shows it. Fewer, fitting languages under *Settings →
  Text Recognition*, and *Recognize Text (OCR)* again, may read it better.

## Office documents are shown as plain text

LibreOffice is not installed, or it could not convert the document. Install LibreOffice
(Writer, Calc and Impress) to have Office documents shown as they look.

## A script does not run

- Scripts need Python 3 and bubblewrap. The list of scripts, under the *Automation* tab of
  the sidebar, says if one is missing and which package to install; start Indeks again
  afterwards. The Flatpak needs neither.
- In a database other than the local one, a script must first be allowed on this computer.
  Open it in the editor and choose *Allow on This Computer*.
- *Show Script Output* lists the errors a script ran into.
- A script that needs more than 30 seconds for one document is stopped.

## A to-do is not in the calendar

- Only to-dos that are open and have a deadline are in the calendar.
- The calendar is turned on for each database: *Settings → To-Do List*, with that database
  open.
- A calendar program shows only the calendars that are ticked in its list of calendars.
  Look there for *Indeks To-Dos*. The calendar of the clock in the panel has such a list in
  its own settings.
- *Show the Calendar File* under *Settings → To-Do List* leads to the file. If the settings
  say there that the file could not be written, the folder of the database cannot be written
  to, or a file called `to-dos.ics` that is not from Indeks is in the way; move that one
  elsewhere.

## Indexing is slow or uses too much of the computer

- Text recognition is the slowest part. Lower *Pages read at the same time* under *Settings →
  Text Recognition*, or switch recognition off.
- Lower *Files read at the same time* under *Settings → Indexing*.
- Closing the window stops all of this at once. What is indexed so far is kept, and the rest
  is picked up at the next start.

## The database cannot be opened

- **Not connected:** The folder of the database cannot be reached. Indeks opens the local
  database instead.
- **In use:** Another computer has the database open. Close Indeks there.
- **Made by a newer version:** A database that a newer version of Indeks has opened cannot be
  opened by an older one.
- **Not an Indeks database:** The file `index.sqlite` in the folder belongs to another
  program, or is no database. Indeks leaves it alone. Choose the `index.sqlite` of an Indeks
  database, or start a new database in another folder.

## Starting over

To have Indeks read a document again, choose *Read Again* from its context menu. To reset
what the list shows, choose *Reset Columns and Rows in All Views* from the column menu.

Removing a location and adding it again reads all its files anew, but documents without
anything attached are forgotten in between, and all of its files count as already there.
