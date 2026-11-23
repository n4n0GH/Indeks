<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Automation

Rules file documents by themselves. Scripts are small programs for what rules cannot express.

Both are found under the *Automation* tab of the sidebar. Choose *Rules* or *Scripts* there
and the middle of the window lists them; the one you select is shown where the preview is
otherwise, to be changed there. `Ctrl+5` goes to the rules, `Ctrl+6` to the scripts.

- **Save** in the bar above stores what you changed. Nothing changes before that.
- **Discard Changes** returns to what is stored.
- **Leaving:** Going back to the documents, or from rules to scripts, loses nothing: what
  you were changing is still there when you return. Indeks asks whether to save when you
  select another rule or script in its place, and before it closes.
- **The pane is hidden with the preview** (`F10`). A double click on a rule or a script
  shows it again.

## Rules

A rule says which documents it is for and what is to happen to them.

### Making a rule

Choose *New → New Rule…*, or *New Rule…* above the list of rules. The new rule has a name and
two lists, and is stored once you choose *Save*.

**For documents that match all of this.** *Add Filter* offers the same filters as the filter
bar: name, text, notes, folder, extension, type, dates, size, tag, group, location and state.
*Search Words…* takes words and search terms as the search field does. Each filter is a line
in the list, with buttons to change and remove it. Below the list Indeks says how many
documents match at the moment; *Show Matching Documents* in the bar lists them.

The choices in these menus say how many documents each would leave. Unlike in the filter
bar, a choice that leaves none can be made here: a rule is also for documents that are yet
to come.

**Do this, in this order.** *Add Action* offers:

- *Add Tag* and *Remove Tag*
- *Add to Group* and *Remove from Group*
- *Pin* and *Unpin*
- *Remove from the Inbox*
- *Run Script*

The lists of tags and groups end with a way to make a new one. *Run Script* offers the
scripts there are; write one under *Scripts* first. The actions are carried out from the top
down. Drag a line
by its handle, or use its arrows, to change the order. Adding the opposite of an action that
is already there replaces it.

**When.** *Automatically, when a document is added* has the rule run on every new document.

### A rule for the inbox only

A rule can be kept to what still waits in the inbox. This suits rules that sort what turns
up in your indexed locations.

1. Under *Add Filter*, choose *State → In the inbox*, next to the filters that describe the
   documents. It can be chosen while the inbox is empty.
2. Have the rule take the document out of the inbox: add *Remove from the Inbox*, or file it
   with *Add to Group*, which does the same. *Add Tag* and *Pin* alone leave it there.

Such a rule lets alone what is filed already, by it or by you. *Show Matching Documents*
lists only the documents it has still to deal with, and not everything it ever sorted.

> **Note:** What is in a folder when you make it a location does not go to the inbox, so
> such a rule does not touch it. See *In and Out → The inbox and new files*.
*Rule is active* switches the rule on and off without deleting it.

### Running a rule

- **By itself:** On documents as they are added, if set so.
- **By hand:** Select the rule in the list of rules. *Apply Selected Rule*, in the bar above
  the rule, its context menu or the command bar, runs it on every document it matches. It
  runs the rule as it is stored, so save your changes first.

**The list of rules** says under each name what the rule is for, and at the right whether it
runs by itself (*Automatic*), waits to be applied (*By hand*) or is switched off (*Not
active*).

To delete a rule, select it and press `Del`, or choose *Delete Rule…* in the bar or the
context menu. What the rule did to documents so far stays.

## Scripts

A script is a small Python program that looks at documents and says what should happen to
them. Scripts need Python 3 and bubblewrap on the computer; without them Indeks says so, and
which package to install. The Flatpak needs neither: it brings Python along and has Flatpak
make the sandbox.

### Writing a script

*New → New Script…*, or *New Script…* above the list of scripts, begins a script with a
starting point:

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

- **Try** runs what is in the editor on the document that is selected in the list of
  documents, and shows what the script would do and what it printed, without doing it.
  Select the document before you go to the scripts; the button names it when the pointer
  rests on it.
- **Run** runs the saved script on the documents that are selected in the list of documents.
- **Reference** lists everything a script can read and ask for. The *Appendix* has the same
  list.
- **Move Script to Trash…**, also `Del` in the list, moves the file of the script to the
  trash of your desktop.

Above the list, *Open Folder* shows the folder the scripts are kept in, and *Show Script
Output* what they printed.

### Running a script

- **On the selected documents:** *Run Script* in the context menu of the documents, *Run
  Script…*, which lists the scripts to pick from, or the command bar, where each script has
  an entry *Run Script “name”* that can be given a keyboard shortcut. In the list of scripts,
  each has a button that runs it on the documents selected before.
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

A script can use what comes with Python, and the Python packages installed for your user
(`~/.local/lib`). In the Flatpak it is what comes with Python only.

### Where scripts are kept

Scripts are `.py` files in the folder `scripts` of the database. They travel with the
database, and you can write them in any editor; Indeks notices changes.

### Scripts in shared databases

In a database other than the local one, a script only runs on a computer after its exact text
was allowed there: by saving it in Indeks on that computer, or with *Allow on This Computer*
after reading it. Otherwise anyone who can write to a shared folder could have a program run
on everyone's computer.
