// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

/**
 * What a plain text file is written as, going by what is in it.
 *
 * A file called `.txt` may well be Markdown, or values in rows and columns. The preview
 * shows such a file as what it is, and as it is in the file on request. Guessing wrong
 * costs the reader a click, so the guess is a careful one: text that only might be either
 * stays text. Markdown that is taken for prose loses nothing but its looks; prose that is
 * taken for Markdown loses its line breaks.
 */
namespace Indeks::TextFormat
{

enum class Guess {
    Plain, ///< text, to be shown as it is
    Markdown,
    Table, ///< values in rows and columns, as in a CSV or TSV file
};

/**
 * Says what @p text is written as. The beginning of a file is enough: its first lines say
 * as much as the rest, and no more than the first few hundred are looked at.
 */
Guess guess(const QString &text);

}
