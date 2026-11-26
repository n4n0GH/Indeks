<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Organising

Indeks gives you several ways to organise documents. They can be combined freely: a document
can be in several groups, carry several tags, be pinned, be linked to other documents, and
have notes, all at once. None of this changes the file.

## The inbox

The inbox holds documents that still need your attention: documents you created, and files
that turned up in an indexed location.

**Working with a document takes it out of the inbox.** Any of the following does:

- putting it into a group,
- giving it a tag,
- pinning it,
- linking it to another document,
- writing a note on it.

*Remove from Inbox* in the context menu takes a document out without any of that. *Move to
Inbox* puts a document back.

> **Note:** Rules and scripts do only what they say. A rule that tags a new document leaves it
> in the inbox, unless the rule also files it into a group or has the action *Remove from the
> inbox*.

## Documents stay in view

A document you are working with stays in the list in front of you, also when your work means
it no longer belongs there. Tag a document under *Untagged*, pin one in the inbox, or unpin
one under *Pinned*: it stays where it is, shown faded and in italics, and the count in the
sidebar changes at once.

Such documents leave the list when you select another entry in the sidebar or change the
search. This way you can do several things to a document in a row.

Commands whose purpose is to take a document out of a list do so at once: *Remove from
Inbox*, *Remove from This Group*, *Move to Trash* and *Restore from Trash*.

## Groups

A group collects documents, like a folder. Unlike a folder, a document can be in several
groups at once, and putting it into a group does not move its file.

- **New group:** *New → New Group…* (`Ctrl+Shift+G`). A group can be created inside another.
- **Adding documents:** *Add to Group…* (`Ctrl+G`), or drag documents onto the group.
- **Removing documents:** *Remove from This Group* while the group is selected, or the cross
  on the group in the *Information* tab.
- **Changing a group:** Right-click it for *Edit…*, which renames it, *New Subgroup…* and
  *Delete…*. Deleting a group does not delete its documents.

## Tags

A tag is a word you attach to documents. Use tags for what cuts across groups, such as
`urgent` or `tax`.

- **Tagging:** *Add Tag…* (`Ctrl+T`), the tag field in the *Information* tab, or drag
  documents onto a tag in the sidebar. A tag that does not exist yet is created.
- **Removing a tag:** Click the cross on the tag in the *Information* tab.
- **Changing a tag:** Right-click it in the sidebar for *Edit…* and *Delete…*.

## Pinning

*Pin* (`Ctrl+Shift+F`) marks documents you want at hand. They are listed under *Pinned* in the
sidebar (`Ctrl+3`) and carry a pin in every list. Choosing *Pin* on pinned documents unpins
them.

## Linked documents

A link says that two documents belong together: a contract and its invoices, a letter and
the reply to it, a scan and the notes you took on it. A link has no direction. It shows on
both documents, and either of them leads to the other. A document can be linked to any
number of others, wherever their files are.

**Linking.**

- *Manage Linked Documents…* (`Ctrl+L`), in the context menu of a document and in the
  *Information* tab, opens a dialog for the selected document. Type into its field to find
  the document to link: part of a name, words from the text, or search terms such as
  `tag:tax`. *Link* beside a document that was found links it. Documents that are linked
  already are marked *Linked*.
- *Link Selected Documents*, in the context menu and in the *Information* tab while several
  documents are selected, links every one of them with every other. This takes up to 20
  documents at a time.

**Following a link.** The *Information* tab lists the documents the selected one is linked
to. Click a name to go to that document. In the dialog, the arrow beside a linked document
does the same.

**Removing a link.** Click the button beside the linked document, in the *Information* tab or
in the dialog. Only the link goes; both documents stay.

**Finding linked documents.** `is:linked` in the search field, or *State → Linked to other
documents* in the filter bar, lists the documents that have a link.

A document in the trash keeps its links, and they say that it is in the trash. Emptying the
trash removes them with the document.

> **Note:** Links belong to what you attached to a document, like tags and notes. A document
> whose file is gone, or whose location is no longer indexed, is kept for as long as it is
> linked; see *In and Out*.

## Comments

Every document has a comment: a free text in the *Information* tab. Use it for a remark about
the document as a whole. Comments are searchable, and the *Comment* column can show them in
the list.

## Notes

A document can have any number of notes. Each note is a text of its own with the time it was
written. Notes are kept in the index, never in the file, and are searchable.

- **Adding:** *Add Note…* in the bar above the preview, in the context menu or the
  *Information* tab (`Ctrl+Shift+M`).
- **Reading and removing:** The *Information* tab lists the notes of the selected document,
  each with a button to delete it.

**Comment or note?** A comment is one short remark that describes the document. Notes are
for what you think or find out while working with it, one entry at a time.

## Sticky notes

On a PDF, a picture or an Office document, a note can be stuck to a place on the page.

- **Adding:** Right-click the place and choose *Add Note Here*. Or choose *Add Note → Add
  Sticky* in the bar above the preview and then click the place. The bar says *Click where
  the note is to go* until you do; *Cancel Adding Sticky* in the same menu calls it off.
- **Reading:** The note shows as a small yellow square. Rest the pointer on it to read it.
- **Changing:** Click the square to open the note. `Ctrl+Enter`, or a click beside it, puts
  it away. A note that is emptied is removed.
- **Moving:** Drag the square to another place on its page.

Sticky notes are listed in the *Information* tab with the page they are on.

## Drawing

On PDFs, pictures and Office documents you can draw with a pen and a highlighter.

1. Click *Draw on Document* in the bar above the preview (`Ctrl+E`).
2. Choose the tool, the colour and the width in the bar that appears.
3. Draw. *Undo* takes back the last stroke.
4. Click *Done*.

The first stroke makes Indeks keep a copy of the file, so that the drawing always fits what
it was made on. The drawing itself is stored in the index. *Remove All Drawings* in the menu
takes it off again.

## Editing text

*Edit Text* (`Ctrl+E`) edits plain text, Markdown, source code and HTML documents in the
preview.

Indeks never writes to an indexed file. When you save, your version is stored as a copy in
Indeks' own folder, and the document shows that copy from then on. *Discard Copy and Return
to Original* drops the copy.

Documents you created in Indeks are edited directly.

## To-dos

The *To-Do* tab of the side panel is a list of things to do. A to-do is a line of text. It
can have a day it is due and a document it is about.

**Creating a to-do.**

- *New → New To-Do…* in the toolbar, or *New To-Do…* at the top of the list.
- *Create To-Do…* in the context menu of a document. The to-do is then linked to that
  document.

The dialog takes the text, the deadline and the document:

- **To do:** What is to be done.
- **Due:** Click the button to pick a day from a calendar. The arrow beside it offers
  *Today*, *Tomorrow*, *In a Week* and *In a Month*. The cross removes the deadline.
- **About:** The linked document. Buttons beside it remove the link or link the document
  that is selected in the list.

**The list.** To-dos are sorted by deadline, the nearest first, followed by those without a
deadline and then those that are done.

- A to-do that is due today or within the next days is highlighted in **orange**. How many
  days ahead is set under *Settings → To-Do List*; at first it is three.
- A to-do that is past its day is highlighted in **red**.
- A to-do with a linked document shows the document's icon. Click the line to show that
  document, wherever it is: Indeks goes to *All Documents*, or to the trash if it is
  there, and selects it. A message of a mailbox is shown in its mailbox, which is opened
  for it. Among very many documents, of which the list holds the first 20,000, a document
  that comes later is listed after the last of those.

**Finishing and changing.**

- Tick the box to mark a to-do as done. It is struck out and moves to the end.
- The pencil that appears on a line, or a double click on a line without a document, opens
  it for changing.
- The context menu has *Show Document*, *Mark as Done*, *Change…* and *Delete*.
- *Remove Completed To-Dos*, at the top right of the list, removes all that are done.

> **Note:** A document with an open to-do counts as worked with, like one with notes. If the
> document leaves Indeks, the to-do stays as a line of text.

### To-dos in the calendar

Your calendar program can show the to-dos that have a deadline, each on the day it is due.
Turn on *Keep the to-dos that have a deadline in a calendar* under *Settings → To-Do List*,
or choose *Keep To-Dos with a Deadline in a Calendar* in the command bar.

- **What is in it:** Every to-do that is open and has a deadline, with the document it is
  about. A to-do leaves the calendar when you tick it off, delete it or take its deadline
  away, and returns when you untick it. To-dos without a deadline are not in it.
- **The system calendar:** Where the calendar of KDE is installed, Indeks adds the to-dos to
  it as a calendar of their own, *Indeks To-Dos* with the name of the database. KOrganizer,
  Merkuro and the calendar of the clock in the panel show it. *Show the calendar in the
  system calendar* in the settings takes it out again and puts it back; where there is no
  such calendar, the switch is not there.
- **Other calendar programs:** The calendar is a file, `to-dos.ics`, in the folder of the
  database. Add it to your calendar program as a calendar from a file. *Show the Calendar
  File* in the settings leads to it.
- **Each database has its own:** The calendar is turned on for the database that is open.
  The to-dos of a database stay in the calendar while another database is open.

Turning it off takes the calendar out of the system calendar and removes the file.

> **Note:** The calendar goes one way, from Indeks to the calendar program. Indeks writes the
> file anew whenever the to-dos change, so what is changed in a calendar program does not
> last. Change and finish to-dos in Indeks.
