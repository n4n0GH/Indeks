// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QStringList>

namespace Indeks
{

/**
 * The passage of @p text around the first place one of @p terms occurs, as HTML with every
 * hit in the passage set in bold. Empty if none of them occurs.
 *
 * Terms are found the way the full-text index finds them: from the beginning of a word,
 * whatever the case and the accents. A term of several words has to be met word for word.
 *
 * This is what the list of search results shows under a document's name. The index has a
 * function for it too, but that one takes the whole text apart again for every document and
 * needs seconds for a large one; this only reads on until the first hit.
 */
QString excerpt(const QString &text, const QStringList &terms, int before = 40, int after = 110);

}
