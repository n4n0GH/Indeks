<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Tasks

This chapter shows how the parts of Indeks work together for some common jobs. Each section
is a short recipe that points to the chapters with the details.

## Going paperless

You scan your paper mail and want to find every letter again.

1. Have your scanner save into one folder, and add that folder as an indexed location
   (*Add → Add Indexed Folder…*).
2. Scan. Each new file appears in Indeks and waits in the *Inbox*.
3. Indeks recognises the text of scans in the background, so that you can search for words
   on the paper. This needs Tesseract; see *Settings → Text Recognition*.
4. Go through the inbox: tag each scan, put it into a group, or pin it. Doing any of these
   takes the document out of the inbox.

To have Indeks do step 4 for you, make a rule, for example: documents whose text contains
`invoice` get the tag `invoice` and go into the group *Finance*. See *Automation*.

## Handling e-mail

You have exported e-mails and want them searchable next to your other documents.

- Single messages saved as `.eml` files are documents like any other. The preview shows the
  sender, the recipients, the text and the attachments.
- A mailbox file (mbox), as mail programs and Google Takeout export them, opens like a folder
  in the list. Each message in it is a document that you can tag, group and annotate.
- Attachments can be opened from the preview or added to Indeks as documents of their own.

Search for `kind:email` to list messages, and add words to narrow the list down.

## Keeping track of what is to be done

A letter needs an answer by the end of the month.

1. Right-click the document and choose *Create To-Do…*.
2. Type what is to be done and pick the day it is due.

The to-do appears in the *To-Do* tab of the side panel. It turns orange when the day is near
and red when it has passed. A click on it brings the letter back into view. Tick it off when
you are done.

## Keeping papers together

A contract, its invoices and the letters about it are separate files, perhaps in separate
folders.

1. Select them in the list, with `Ctrl`-click or `Shift`-click.
2. Right-click and choose *Link Selected Documents*.

From now on the *Information* tab of each of them lists the others, and a click on a name
goes there. To add a paper later, select one of them, choose *Manage Linked Documents…*
(`Ctrl+L`) and look for the new one. See *Organising → Linked documents*.

## Keeping something you read or see

A paragraph on a web page, a few lines of a message, or what is on the screen right now is
worth keeping, and there is no file of it.

1. Copy it where you found it: select the text and press `Ctrl+C`, or take a screenshot to
   the clipboard.
2. In Indeks, choose *New → New Document from Clipboard…* (`Ctrl+Shift+V`).
3. Give it a name and choose *Create Document*.

The document is stored in Indeks and waits in the inbox. See *Documents → Documents
created in Indeks*.

To keep only where a page is, copy its address from the address bar of the browser instead.
The same command then makes a website: a document that opens the page in your browser, and
that takes tags, notes and links like any other. See *Documents → Websites*.

## Filing automatically

You get the same kinds of documents again and again.

1. Choose *New → New Rule…*. The rule opens under the *Automation* tab of the sidebar.
2. Give it a name. Under *For documents that match all of this*, add filters that describe
   the documents.
3. Under *Do this, in this order*, add what should happen: a tag, a group, a pin.
4. Leave the option to run the rule on new documents ticked, and choose *Save*.

To have the rule deal with new arrivals only, add the filter *State → In the inbox* and the
action *Remove from the Inbox*. See *Automation → A rule for the inbox only*.

From then on, every document that arrives and matches is filed by the rule. For anything a
rule cannot express, such as filing by a year found in the text, write a script.

## Finding a document again

- You remember a word: type it into the search field.
- You remember roughly when: open the filter bar (`Ctrl+Alt+F`) and choose a date.
- You remember what kind it was: add the filter *File Type*.
- You need this search often: *Save Search…* (`Ctrl+Shift+S`) puts it into the sidebar.

## Working on a document

- **Editing text.** Select a text document and click *Edit Text* in the bar above the preview
  (`Ctrl+E`). Indeks edits a copy; the original file is not changed.
- **Marking a PDF or picture.** Click *Draw on Document* in the same bar. Use the pen or the
  highlighter. The drawing is kept in the index.
- **Sticking a note to a page.** Choose *Add Note → Add Sticky* and click where the note
  should go.

## Using Indeks on several computers

Start a database in a folder that all computers can reach, for example on a NAS, with *New
Database…* on one of them. On the others, open it with *Add Database…* and choose its file
`index.sqlite`. Only one computer can have the database open at a time.
See *Databases* for what to keep in mind.
