<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Keeping the documentation

Indeks has a manual for the people who use it and this documentation for the people who
change it. Both live in the repository and are changed in the same step as the code.

## The rule

**A change that someone using Indeks can see or feel changes the manual too**, in the same
piece of work:

1. Change the chapter that describes the part: what it is called, where it is, what it does.
2. If a command, a shortcut, a search term, a setting or something a script can call was
   added, renamed or removed, change the tables in the *Appendix* as well.
3. Add a line to *Changes*, at the top of the list of the current version.
4. If a picture in `manual/images` shows the part and no longer matches, replace it.
5. Build, and look at the pages you changed in `build/doc/Indeks-Manual.pdf`.

A change to how Indeks is built (a new class, a table of the database, a convention) changes
`doc/developer` in the same way.

What does not need the manual: changes nobody using Indeks can notice, such as refactoring
or faster code.

## The manual

### Where it is

`doc/manual/` holds one Markdown file per chapter. The files are put together in the order
of their names, so each name begins with a number. The first heading (`# Title`) is the title
of the chapter; `##` headings are its sections and show in the table of contents; `###` are
subsections.

| File | Chapter |
| --- | --- |
| `01-getting-started.md` | What Indeks is, first steps |
| `02-tasks.md` | Recipes for common jobs |
| `03-documents.md` | The kinds of documents and how each is shown |
| `04-in-and-out.md` | Locations, adding files, the inbox, the trash, missing files |
| `05-window.md` | Toolbar, sidebar, list and columns, preview, side panel, command bar |
| `06-finding.md` | Search field, search terms, filter bar, saved searches, smart groups |
| `07-organising.md` | Inbox, groups, tags, pins, comments, notes, drawing, editing, to-dos |
| `08-automation.md` | Rules and scripts |
| `09-settings.md` | Every page of the settings |
| `10-databases.md` | Databases, network shares, where things are kept |
| `11-troubleshooting.md` | What to check when something goes wrong |
| `12-appendix.md` | Glossary, shortcuts, commands, search language, script reference |
| `13-changes.md` | What is new, the latest first |

### How it is written

- For people who use Indeks, not for people who know how it is made. Say what to do and what
  happens; leave out how it works inside unless it explains something they will notice.
- Short sentences, second person ("you"), present tense.
- Names of things in the interface in *italics*, exactly as they are on the screen,
  ellipsis included: *Add Tag…*. Menu paths with arrows: *New → New To-Do…*.
- Keys and things to type in `code`: `Ctrl+T`, `tag:invoice`.
- Things of a kind as a list in which each entry starts with its name in bold:
  `- **Order:** Drag a title sideways.`
- Something that is easy to miss as a note: `> **Note:** …`
- One place for each fact. Other chapters point to it (*see Organising → To-dos*) instead of
  saying it again; only the *Appendix* repeats, as tables.
- Describe only what Indeks does. If you are not sure that something works as you are about
  to write, try it or read the code first.

### Pictures

Pictures are PNG files in `doc/manual/images`, put in with
`![What it shows](images/name.png)`; the text in brackets becomes the caption. A picture
that fits a column, or nearly, stands in the column where it is put. A wider one, such as
a picture of the whole window, is shown across both columns at the top of the next page. They are
taken from an Indeks with made-up documents, never from real ones, at the size of the window
at 96 dpi. Keep them few: every picture has to be taken again when the part it shows
changes.

### Building it

The manual is built with Indeks, by `indeks-manualbuilder` (`doc/tools/manualbuilder.cpp`):

```sh
cmake --build build                  # or: cmake --build build --target manual
xdg-open build/doc/Indeks-Manual.pdf
```

The builder needs nothing but Qt. It lays the chapters out on A4 in two columns, starts each
on a new page under its title with a list of its sections, puts a cover and a table of
contents in front, and writes the chapter and the page at the foot of every page. Every line of the table of contents and of
the lists at the start of the chapters is a link to its place, and the chapters and sections
are bookmarks (the outline of the PDF). Qt's PDF writer knows neither, so the builder adds
them to the finished file itself, as an update appended to it (`PdfUpdate` in the builder). The cover names the version (the one of the
project in `CMakeLists.txt`) and the day the chapters were last changed.

What it understands is what Qt reads as Markdown: headings, paragraphs, lists, tables, code
blocks, block quotes, pictures, bold, italics and code. Raw HTML and footnotes are not for
it.

Installing puts the PDF into `share/indeks/`. *Indeks Manual* (`F1`) opens the installed one,
or the one in the build folder when Indeks runs from there.

## The developer documentation

`doc/developer/` is Markdown that is read as it is, in an editor or on the web.

| File | About |
| --- | --- |
| `architecture.md` | The programs, classes, threads and processes, and the principles the code keeps to |
| `database.md` | The tables of the index and how to change them |
| `development.md` | Building, testing, conventions, a checklist for new features |
| `documentation.md` | This file |

## The README

`README.md` at the top says what Indeks is, how to build it and where the documentation is.
It does not describe features in detail; that is the manual's job.
