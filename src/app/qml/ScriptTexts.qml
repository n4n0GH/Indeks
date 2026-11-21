// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick

// Words about scripts that more than one dialog needs: what a script asked for, how a run
// went, and what a script has to work with.
QtObject {
    id: root

    // What a script asked to be done to a document, as a line of text. With @p would, as
    // something that was only tried and not done.
    function actionText(action: var): string {
        const group = (action.path ?? []).join(" / ");
        switch (action.type) {
        case "tag":
            return i18nc("@item what a script asked for", "Add the tag “%1”", action.name);
        case "untag":
            return i18nc("@item what a script asked for", "Remove the tag “%1”", action.name);
        case "group":
            return i18nc("@item what a script asked for", "Add to the group “%1”", group);
        case "ungroup":
            return i18nc("@item what a script asked for", "Remove from the group “%1”", group);
        case "flag":
            return i18nc("@item what a script asked for", "Pin");
        case "unflag":
            return i18nc("@item what a script asked for", "Unpin");
        case "inbox":
            return i18nc("@item what a script asked for", "Remove from the inbox");
        case "toinbox":
            return i18nc("@item what a script asked for", "Move to the inbox");
        case "comment":
            return action.text.length > 0 ? i18nc("@item what a script asked for", "Set the comment to “%1”", action.text)
                                          : i18nc("@item what a script asked for", "Remove the comment");
        case "note":
            return i18nc("@item what a script asked for", "Attach the note “%1”", action.text);
        }
        return action.type;
    }

    // How a run went, in one line.
    function summary(report: var): string {
        if (report.problem.length > 0) {
            return i18nc("@info how a script's run went", "Did not get through");
        }
        const parts = [i18ncp("@info how a script's run went", "%1 document", "%1 documents", report.documents)];
        if (report.changed > 0) {
            parts.push(i18ncp("@info how a script's run went", "%1 changed", "%1 changed", report.changed));
        }
        if (report.failed > 0) {
            parts.push(i18ncp("@info how a script's run went", "%1 failed", "%1 failed", report.failed));
        }
        if (report.stopped) {
            parts.push(i18nc("@info how a script's run went", "stopped"));
        }
        return parts.join(i18nc("separates the parts of a summary", ", "));
    }

    // What a script has to work with. Names of things in Python stay as they are.
    readonly property string reference: [
        i18nc("@info script reference", "A script is a Python file with a function process(document). Indeks calls it once for every document the script is run on. The script looks at the document and says what is to happen to it; Indeks then does that."),
        "",
        i18nc("@title script reference", "Looking at a document"),
        "  document.name          " + i18nc("@info script reference", "the file name"),
        "  document.extension     " + i18nc("@info script reference", "its ending in small letters, without the dot"),
        "  document.text          " + i18nc("@info script reference", "the text Indeks found in it, recognized text of scans included"),
        "  document.path          " + i18nc("@info script reference", "the file to read; None while it is not on this computer"),
        "  document.url           " + i18nc("@info script reference", "where the original is"),
        "  document.kind          \"text\", \"rich\", \"pdf\", \"image\", \"email\", \"canvas\", \"other\"",
        "  document.mime_type     " + i18nc("@info script reference", "for example \"application/pdf\""),
        "  document.size          " + i18nc("@info script reference", "in bytes"),
        "  document.pages         " + i18nc("@info script reference", "of a PDF"),
        "  document.modified      " + i18nc("@info script reference", "a datetime; document.added likewise"),
        "  document.location      " + i18nc("@info script reference", "the name of the indexed location it is in"),
        "  document.tags          " + i18nc("@info script reference", "a list of names; also document.has_tag(\"name\")"),
        "  document.groups        " + i18nc("@info script reference", "a list of tuples like (\"Finance\", \"Invoices\"); also document.in_group(\"Finance\", \"Invoices\")"),
        "  document.pinned, document.inbox, document.comment, document.notes",
        "",
        i18nc("@title script reference", "Saying what is to happen"),
        "  document.add_tag(\"name\")",
        "  document.remove_tag(\"name\")",
        "  document.add_to_group(\"Finance\", \"Invoices\")       " + i18nc("@info script reference", "groups that are missing are created"),
        "  document.remove_from_group(\"Finance\", \"Invoices\")",
        "  document.pin()   document.unpin()",
        "  document.remove_from_inbox()   document.move_to_inbox()",
        "  document.comment = \"…\"",
        "  document.add_note(\"…\")",
        "",
        i18nc("@info script reference", "What print() writes is kept in the script output. If the script raises an error on a document, nothing is done to that document."),
        "",
        i18nc("@info script reference", "Scripts run in a sandbox. There is no network. Files can be read but not written, and the only ones there are the documents and what Python itself is made of. A script may take up to 30 seconds over one document."),
    ].join("\n")
}
