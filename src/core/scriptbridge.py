# SPDX-FileCopyrightText: 2026 n4n0
# SPDX-License-Identifier: GPL-2.0-or-later

"""Runs a script that is stored in Indeks on documents.

Indeks starts this with the Python of the system, inside a sandbox, and talks to it in lines of
JSON: the script first, then one document after the other. What the script wants done with a
document is collected and sent back, and it is Indeks that does it. Nothing in here is able to
change a document, the index or a file.

Compiled into Indeks (see ScriptProcess); it has to run on every Python 3 still in use.
"""

import io
import json
import linecache
import os
import sys
import traceback
import types
from datetime import datetime

# The channel to Indeks is what standard input and output were at the start. Whatever the script
# or a library prints must not end up in it, so print() is pointed elsewhere from here on.
_to_indeks = os.fdopen(os.dup(1), "w", encoding="utf-8", newline="\n")
_from_indeks = io.TextIOWrapper(os.fdopen(os.dup(0), "rb"), encoding="utf-8", newline="\n")
os.dup2(2, 1)
_null = os.open(os.devnull, os.O_RDONLY)
os.dup2(_null, 0)
os.close(_null)

# The folder Indeks happened to be started from is no place to import from.
sys.path[:] = [entry for entry in sys.path if entry not in ("", ".")]

_OUTPUT_LIMIT = 64 * 1024


def _send(message):
    _to_indeks.write(json.dumps(message))
    _to_indeks.write("\n")
    _to_indeks.flush()


def _receive():
    line = _from_indeks.readline()
    if not line:
        # Indeks has closed the channel: there is nothing more to do.
        sys.exit(0)
    return json.loads(line)


def _name(value, what):
    if not isinstance(value, str):
        raise TypeError("%s must be a string, not %s" % (what, type(value).__name__))
    value = " ".join(value.split())
    if not value:
        raise ValueError("%s must not be empty" % what)
    return value


def _path(parts):
    if not parts:
        raise TypeError("name the group, each level as an argument of its own")
    return [_name(part, "the name of a group") for part in parts]


class Document:
    """A document in Indeks as a script sees it.

    Reading costs nothing and changes nothing. What the methods ask for is carried out by
    Indeks once process() has returned without an error.
    """

    def __init__(self, data):
        self._actions = []
        self._text = None
        self.id = data["id"]
        self.name = data["name"]
        self.path = data.get("path")
        self.url = data["url"]
        self.mime_type = data["mime"]
        self.kind = data["kind"]
        self.size = data["size"]
        self.modified = datetime.fromtimestamp(data["modified"])
        self.added = datetime.fromtimestamp(data["added"])
        self.pages = data.get("pages", 0)
        self.location = data.get("location", "")
        self._tags = list(data.get("tags", []))
        self._groups = [tuple(group) for group in data.get("groups", [])]
        self._flagged = bool(data.get("flagged"))
        self._inbox = bool(data.get("inbox"))
        self._comment = data.get("comment", "")
        self._notes = list(data.get("notes", []))

    def __repr__(self):
        return "<Document %r>" % self.name

    # ------------------------------------------------------------------ reading

    @property
    def extension(self):
        """The file name extension in small letters, without the dot."""
        return os.path.splitext(self.name)[1][1:].lower()

    @property
    def text(self):
        """The text Indeks found in the document, recognized text of scans included."""
        if self._text is None:
            _send({"need": "text"})
            self._text = _receive().get("text", "")
        return self._text

    @property
    def tags(self):
        return list(self._tags)

    @property
    def groups(self):
        """The groups the document is in, each as a tuple of names from the outermost in."""
        return list(self._groups)

    @property
    def notes(self):
        return list(self._notes)

    def has_tag(self, name):
        wanted = _name(name, "the name of a tag").lower()
        return any(tag.lower() == wanted for tag in self._tags)

    def in_group(self, *path):
        wanted = tuple(part.lower() for part in _path(path))
        return any(tuple(part.lower() for part in group) == wanted for group in self._groups)

    # ------------------------------------------------------------------ asking for changes

    def add_tag(self, name):
        name = _name(name, "the name of a tag")
        if not self.has_tag(name):
            self._tags.append(name)
        self._actions.append({"type": "tag", "name": name})

    def remove_tag(self, name):
        name = _name(name, "the name of a tag")
        self._tags = [tag for tag in self._tags if tag.lower() != name.lower()]
        self._actions.append({"type": "untag", "name": name})

    def add_to_group(self, *path):
        """Puts the document into a group: add_to_group("Finance", "Invoices") for the group
        Invoices inside Finance. Groups that do not exist yet are created."""
        path = _path(path)
        if not self.in_group(*path):
            self._groups.append(tuple(path))
        self._actions.append({"type": "group", "path": path})

    def remove_from_group(self, *path):
        path = _path(path)
        wanted = tuple(part.lower() for part in path)
        self._groups = [group for group in self._groups if tuple(part.lower() for part in group) != wanted]
        self._actions.append({"type": "ungroup", "path": path})

    @property
    def pinned(self):
        return self._flagged

    @pinned.setter
    def pinned(self, value):
        self._flagged = bool(value)
        # Pinning was called flagging once, and still is between this and Indeks.
        self._actions.append({"type": "flag" if value else "unflag"})

    def pin(self):
        self.pinned = True

    def unpin(self):
        self.pinned = False

    # The names from when pinning was called flagging; scripts written then go on working.
    flagged = pinned
    flag = pin
    unflag = unpin

    @property
    def inbox(self):
        return self._inbox

    @inbox.setter
    def inbox(self, value):
        self._inbox = bool(value)
        self._actions.append({"type": "toinbox" if value else "inbox"})

    def remove_from_inbox(self):
        self.inbox = False

    def move_to_inbox(self):
        self.inbox = True

    @property
    def comment(self):
        return self._comment

    @comment.setter
    def comment(self, text):
        if not isinstance(text, str):
            raise TypeError("a comment must be a string, not %s" % type(text).__name__)
        self._comment = text
        self._actions.append({"type": "comment", "text": text})

    def add_note(self, text):
        """Attaches a note. A note the document has already, word for word, is not added twice."""
        if not isinstance(text, str):
            raise TypeError("a note must be a string, not %s" % type(text).__name__)
        text = text.strip()
        if not text:
            raise ValueError("a note must not be empty")
        if text not in self._notes:
            self._notes.append(text)
        self._actions.append({"type": "note", "text": text})


class _Output(io.StringIO):
    """What the script prints while it is at one document; cut off when it gets out of hand."""

    def write(self, text):
        room = _OUTPUT_LIMIT - self.tell()
        if room <= 0:
            return len(text)
        super().write(text[:room])
        return len(text)


def _described(error, script_name):
    """The error with the lines of the script that led to it, without those of this file."""
    trace = error.__traceback__
    while trace is not None and trace.tb_frame.f_code.co_filename != script_name:
        trace = trace.tb_next
    if trace is None:
        trace = error.__traceback__
    return "".join(traceback.format_exception(type(error), error, trace)).rstrip()


def _load(name, source):
    # So that error messages can quote the lines of the script.
    linecache.cache[name] = (len(source), None, source.splitlines(True), name)
    module = types.ModuleType("indeks_script")
    module.__file__ = name
    exec(compile(source, name, "exec"), module.__dict__)
    process = module.__dict__.get("process")
    if not callable(process):
        raise LookupError("The script has no function named process(document).")
    return process


def _main():
    script = _receive()
    name = script["name"]
    try:
        process = _load(name, script["source"])
    except LookupError as error:
        _send({"failed": str(error)})
        return
    except BaseException as error:  # a script may do anything at all, sys.exit() included
        _send({"failed": _described(error, name)})
        return
    _send({"ready": True})

    while True:
        document = Document(_receive()["document"])
        output = _Output()
        error = None
        printed_to = sys.stdout, sys.stderr
        sys.stdout = sys.stderr = output
        try:
            process(document)
        except BaseException as caught:
            error = _described(caught, name)
        finally:
            sys.stdout, sys.stderr = printed_to
        _send({
            # Nothing is done to a document the script failed on, not even the first half.
            "actions": [] if error else document._actions,
            "output": output.getvalue(),
            "error": error,
        })


_main()
