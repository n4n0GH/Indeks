<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Documents

A *document* in Indeks stands for a file: one in an indexed location, or one you created in
Indeks. This chapter describes the kinds of documents and how each is shown in the preview.

## General

Every document has a name, a type, a size, the times it was changed and added, and a
location. To these Indeks adds what you attach: tags, groups, a pin, a comment, notes,
drawings and to-dos.

**The preview.** Selecting a document shows it in the preview below the list. The bar above
the preview is always there. On its left it says a word about the document, such as its
number of pages. On its right are the things you can do with it, and at the end the zoom.

**Zoom.** Every kind of document can be shown larger and smaller: with the buttons in the
bar, with `Ctrl` and the mouse wheel, or with `Ctrl++`, `Ctrl+-` and `Ctrl+0`. The number
between the buttons shows the size; a click on it goes back to normal. Pictures and pages
start out fitted to the view. The size of text is remembered.

**Opening elsewhere.** *Open File* (`Ctrl+Return`) opens the document in its usual
application. *Open With…* lets you choose another one. *Show in File Manager* shows the file
in its folder.

## Plain text and source code

Text files are shown as text. Source code and data files get line numbers and syntax colours.
Lines that are too long wrap and continue below their own indentation.

- **Editing:** *Edit Text* (`Ctrl+E`) turns the preview into an editor. *Save* stores your
  changes in a copy that Indeks keeps; *Discard Changes* drops them; *Done* ends editing.
- **Long files:** Of a file larger than 4 MiB the preview shows the first 4 MiB. *Edit Text*
  loads the whole text.
- **Selecting and copying:** With the text in focus, `Ctrl+A` selects all of it and `Ctrl+C`
  copies the selection.

## Markdown and HTML

Markdown and HTML documents are shown formatted. *Show Unformatted* (`Ctrl+U`) in the bar
above the preview shows the source instead.

*Edit Text* works on the source of a Markdown document. An HTML document that is shown
formatted is edited as it looks.

## PDF

PDF documents are shown page by page. The bar above the preview says which page is in view.

- **Selecting text:** Drag with the mouse to select text. The drag can start and end anywhere
  on the page, also in the margin; the nearest text counts. A double click selects a word. A
  single click removes the selection. `Ctrl+C` or the copy button copies it. `Ctrl+A` selects
  the text of the page in view. A selection stays on one page.
- **Search hits:** Coming from a search, the places where the word was found are marked.
- **Drawing and notes:** See *Organising → Drawing* and *Sticky notes*.

> **Note:** A scanned page is a picture of text and has no text to select. Indeks recognises
> its text for searching; see *Settings → Text Recognition*.

## Pictures

Pictures are shown fitted to the preview. The bar says their size in pixels. You can zoom,
draw on them and stick notes to them. Text in pictures is recognised for searching.

## Office documents

Word, Excel and PowerPoint files in old and new formats, OpenDocument files and RTF are shown
as they look, if LibreOffice is installed. Indeks has LibreOffice make a PDF of the document
and shows that, so everything that works on a PDF works here too: selecting text, drawing and
sticky notes.

- The PDF is kept in the cache and removed 90 days after it was last looked at. The document
  itself is only read.
- LibreOffice runs without a window and, where bubblewrap is installed, without access to the
  network.
- Without LibreOffice, Office documents are shown as the plain text Indeks found in them, and
  files in the formats from before 2007 (`.doc`, `.xls`, `.ppt`) are found by name only.

## E-mail

A message saved as an `.eml` file is shown with its subject, sender, recipients and date,
followed by its text and its attachments.

- **Long lists of recipients:** *To*, *Cc* and *Bcc* take one line each. A button such as
  *Show All 70* opens a line that holds more.
- **Formatted messages:** *Show Formatted* switches between the plain and the formatted
  version of a message. Pictures and other content from the network are never loaded.
- **Attachments:** An attachment can be opened, or added to Indeks as a document.

## Mailboxes

A mailbox file (mbox) opens like a folder. Click the arrow before its name, or press the
right arrow key, to list its messages below it. Each message is a document: it is found by
its own words and can be tagged, grouped, pinned, annotated, matched by rules and handed to
scripts.

- **Where messages are listed:** Under their mailbox, and wherever something of their own
  finds them: a search for words, a tag, a group, a pin, a note, the inbox. Lists that go by
  place, type, size or date alone show the mailbox and not the messages in it.
- **Large mailboxes:** A mailbox is never loaded into memory, so its size does not matter. An
  opened mailbox lists up to 20,000 messages; a line above the list says when there are more.
  Searching finds all of them.
- **When the mailbox changes:** After mail was added or deleted, only new messages are read.
  What you attached to the others stays with them.
- **Nothing is written:** The mailbox file is only read.

## Tables: CSV and TSV

Files of values in rows and columns are shown as a table, with the column names at the top,
numbers aligned to the right and the rows numbered as in the file.

- The separator (comma, semicolon, tab or bar) is detected from the first lines.
- *Column Names* switches whether the first row is treated as names.
- Drag the line between two column names to change a column's width.
- Click a cell to make it current; the arrow keys move on from it and `Ctrl+C` copies it.
- *View Raw* (`Ctrl+U`) shows the file as text.

Of a file larger than 48 MiB the table shows the beginning.

## JSON and JSON Lines

JSON Lines files (`.jsonl`, `.ndjson`) and JSON written without line breaks are laid out for
reading: every record is indented with one member per line and can be folded. *Fold All*
turns the file into a list of one-line summaries. Large files show their first 500 records
until you click *Show More*. *View Raw* shows the file as it is.

This is a view only. Nothing in the file is rewritten.

## Obsidian canvases

`.canvas` files are shown as the board they are: text cards, groups, link cards, file cards
and the connections between them. Drag to move around, use `Ctrl` and the wheel to zoom.

- A double click on a file card jumps to that document in Indeks.
- A double click on a link card opens the link after you confirmed where it leads.
- The words on the cards are searchable.
- Canvases are displayed, not edited.

## Documents created in Indeks

*New → New Plain Text Document…*, *New Markdown Document…* and *New Formatted Document…*
create a document that is stored in Indeks' own folder. Such documents can be renamed (`F2`).
They start out in the inbox.

## Documents that cannot be read

If the program that reads files gives up on a file, the document is still added, by its name.
The side panel says that its content could not be read. *Read Again* in the context menu
tries once more.
