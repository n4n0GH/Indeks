<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Organising

Indeks gives you several ways to organise documents. They can be combined freely: a document
can be in several groups, carry several tags, be pinned, and have notes, all at once. None of
this changes the file.

## The inbox

The inbox holds documents that still need your attention: files you added by hand, documents
you created, and files that turned up in an indexed location.

**Working with a document takes it out of the inbox.** Any of the following does:

- putting it into a group,
- giving it a tag,
- pinning it,
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
  document, also when it is in the trash.

**Finishing and changing.**

- Tick the box to mark a to-do as done. It is struck out and moves to the end.
- The pencil that appears on a line, or a double click on a line without a document, opens
  it for changing.
- The context menu has *Show Document*, *Mark as Done*, *Change…* and *Delete*.
- *Remove Completed To-Dos*, at the top right of the list, removes all that are done.

> **Note:** A document with an open to-do counts as worked with, like one with notes. If the
> document leaves Indeks, the to-do stays as a line of text.
