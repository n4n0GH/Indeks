<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Automation

Rules file documents by themselves. Scripts are small programs for what rules cannot express.

## Rules

A rule says which documents it is for and what is to happen to them.

### Making a rule

Choose *New → New Rule…*. The dialog has a name and two lists.

**For documents that match all of this.** *Add Filter* offers the same filters as the filter
bar: name, text, notes, folder, extension, type, dates, size, tag, group, location and state.
*Search Words…* takes words and search terms as the search field does. Each filter is a line
in the list, with buttons to change and remove it. The dialog says how many documents match
at the moment.

**Do this, in this order.** *Add Action* offers:

- *Add Tag* and *Remove Tag*
- *Add to Group* and *Remove from Group*
- *Pin* and *Unpin*
- *Remove from the Inbox*
- *Run Script*

Nothing in these menus is greyed out when there is no tag, group or script yet: each list
ends with a way to make a new one. The actions are carried out from the top down. Drag a line
by its handle, or use its arrows, to change the order. Adding the opposite of an action that
is already there replaces it.

**When.** *Automatically, when a document is added* has the rule run on every new document.
*Rule is active* switches the rule on and off without deleting it.

### Running a rule

- **By itself:** On documents as they are added, if set so.
- **By hand:** Select the rule in the sidebar. The list shows what it matches. *Apply Selected
  Rule*, in the context menu of the rule or the command bar, runs it on all of them.

To change or delete a rule, right-click it in the sidebar.

## Scripts

A script is a small Python program that looks at documents and says what should happen to
them. Scripts need Python 3 and bubblewrap on the computer; without them Indeks says so.

### Writing a script

*Scripts…* in the menu lists your scripts. *New Script…* opens the editor with a starting
point:

```
"""Files invoices by the year they are from."""
import re


def process(document):
    if "invoice" not in document.text.lower():
        return
    document.add_tag("invoice")
    year = re.search(r"\b(20\d\d)\b", document.text)
    if year:
        document.add_to_group("Finance", "Invoices", year.group(1))
```

Indeks calls `process(document)` once for every document the script is run on. The first
line of the description at the top is shown in the list of scripts.

- **Try on Selected Document** runs what is in the editor on the selected document and shows
  what the script would do and what it printed, without doing it.
- **Reference** lists everything a script can read and ask for. The *Appendix* has the same
  list.

### Running a script

- **On the selected documents:** *Run Script* in the context menu, *Scripts…*, or the command
  bar, where each script has an entry *Run Script “name”* that can be given a keyboard
  shortcut.
- **On new documents:** As an action of a rule.

While scripts run, the activity display at the bottom of the sidebar says so. *Stop Running
Scripts* ends them.

**Output.** What a script prints, and the errors it runs into, are kept under *Show Script
Output* until Indeks is closed. If a script fails on a document, nothing is done to that
document.

### What a script can and cannot do

A script can do no more than a rule: it asks, and Indeks carries it out. It cannot change or
delete a file, and it cannot touch the index.

Scripts run in a sandbox:

- no access to the network,
- nothing to write to,
- no way to talk to other programs,
- of your files, only the documents, and those read-only.

A script that takes longer than 30 seconds over one document is stopped.

### Where scripts are kept

Scripts are `.py` files in the folder `scripts` of the database. They travel with the
database, and you can write them in any editor; Indeks notices changes.

### Scripts in shared databases

In a database other than the local one, a script only runs on a computer after its exact text
was allowed there: by saving it in Indeks on that computer, or with *Allow on This Computer*
after reading it. Otherwise anyone who can write to a shared folder could have a program run
on everyone's computer.
