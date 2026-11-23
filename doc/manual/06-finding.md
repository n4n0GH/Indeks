<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Finding

Everything in Indeks can be searched: file names, the text inside documents, your comments
and notes, and text recognised in pictures and scans.

## The search field

Click into the search field or press `Ctrl+F`, and type. The search looks in the place that
is selected in the sidebar; the field says which, for example *Search in Inbox…*.

- **Words** match names, text and notes. A word also matches the beginning of a longer word:
  `pine` finds *pineapple*.
- **Several words** must all be found.
- **A phrase** in quotes is found as written: `"analytical engine"`.
- **A minus** in front excludes: `-draft`.
- **OR** between two terms finds either: `invoice OR receipt`.

The search starts by itself shortly after you stop typing. Under each found document a line
shows where the word occurs. Selecting a document opens the preview at the first hit.

> **Note:** Searching the text of a very large index for a very short word can take a few
> seconds. A line runs along the top of the list meanwhile, and you can keep typing.

## Search terms

Besides words, a search can hold terms that ask for a property. They can be mixed freely with
words and with each other.

| Term | Finds |
| --- | --- |
| `tag:invoice`, `tag:"two words"` | documents with that tag |
| `tag:none` | documents without any tag |
| `group:Taxes` | documents in that group |
| `group:none` | documents in no group |
| `kind:text` `kind:rich` `kind:pdf` `kind:image` `kind:email` `kind:canvas` `kind:website` | documents of a kind |
| `ext:md` | by file name extension |
| `name:report` | a word in the name only |
| `name:*.pdf`, `name:scan_??.png` | a pattern for the whole name: `*` any characters, `?` one |
| `path:Invoices/2026` | part of the path of the folder the file is in |
| `source:Documents` | documents of an indexed location |
| `content:word` | the word in the text only |
| `note:word` | the word in notes and comments only |
| `added:today`, `added:7d`, `added:12h` | added today, in the last 7 days, in the last 12 hours |
| `modified:>2026-01-31`, `added:2026-03` | changed after a day; added in a month |
| `size:>5M`, `size:<200k` | by size |
| `is:inbox`, `is:pinned`, `is:annotated`, `is:linked` | by what you did with them |
| `is:unfiled`, `is:untagged` | in no group; without tags |
| `is:duplicate` | files with the same content as another |
| `is:missing`, `is:trashed` | file is gone; in the trash |
| `is:local`, `is:copied`, `is:ocr` | stored in Indeks; has a working copy; text was recognised |

The same language is used for saved searches and for rules. The *Appendix* has the complete
list.

## The filter bar

The filter button beside the search field (`Ctrl+Alt+F`) opens the filter bar above the list.
There you put a search together by picking, without typing terms.

- *Add Filter* offers: name, text in the document, text in notes and comments, folder,
  extension, file type, date modified, date added, size, tag, group, location and state.
- Each filter you add shows as a chip and narrows the list at once. Click the cross on a chip
  to remove it; click a typed chip to change it.
- Lists of choices show how many documents each choice would leave.
- *Modified Between Dates…*, *Added Between Dates…* and *Other Size…* take values that are
  not in the lists.

Several tags or states can be on at once, and all must apply. Of file type, size, each date
and location there is one at a time; picking another replaces it.

The filters add to what is typed in the search field. Closing the bar removes all filters.

## Saved searches

*Save Search…* (`Ctrl+Shift+S`) keeps the current search, with its filters, under a name. It
appears under *Saved Searches* in the sidebar and always shows what matches at the moment.

To change a saved search, right-click it and choose *Edit…*. *Delete…* removes it; the
documents it found are not affected.

## Smart groups

The smart groups in the sidebar are searches that come with Indeks:

- **Added Today**, **Added This Week:** What is new.
- **Not in a Group**, **Untagged:** What is not organised yet.
- **Annotated:** Documents with a comment, notes or drawings.
- **Duplicates:** Files with the same content as another file in the index.
- **Missing Files:** Documents you restored although their file is gone. This entry is only
  there when there are any.

## Finding inside a document

Coming from a search, the preview marks the search words in the document and opens at the
first hit: in text, on the pages of a PDF and in the cells of a table.
