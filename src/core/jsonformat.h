// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QList>
#include <QString>

/**
 * Lays JSON out for reading. Nothing is parsed into values and written again: the text is only
 * broken into lines and indented, so keys keep their order, numbers their spelling and broken
 * or cut-off JSON comes out as far as it goes.
 */
namespace Indeks
{

/** JSON with one member or element per line, indented by @p indentWidth spaces per level. */
QString reindentJson(QStringView json, int indentWidth = 2);

/** Whether @p fileName is a JSON Lines file: one JSON value per line. */
bool isJsonLinesFile(const QString &fileName);

/** Whether JSON text is written without line breaks, as programs do for each other. */
bool isMinifiedJson(QStringView json);

struct FormattedJson {
    QString text;
    /** Lines of @p text (from 0) at which a record begins, … */
    QList<int> recordLines;
    /** … and the line of the file (from 1) that record is. */
    QList<int> sourceLines;
    /** Whether records were left out because there were more than asked for. */
    bool cut = false;
};

/**
 * Formats a JSON Lines text record by record. Lines that are not JSON objects or arrays are
 * kept as they are. Stops after @p maxRecords records (0: no limit) or @p maxCharacters of
 * input (0: no limit), whichever comes first.
 */
FormattedJson formatJsonLines(const QString &text, int maxRecords = 0, qsizetype maxCharacters = 0, int indentWidth = 2);

}
