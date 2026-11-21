// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jsonformat.h"

using namespace Qt::StringLiterals;

namespace Indeks
{

namespace
{
/** Appends @p json re-indented to @p out and returns the number of line breaks added. */
int reindentInto(QStringView json, int indentWidth, QString &out)
{
    int lineBreaks = 0;
    int depth = 0;
    const qsizetype n = json.size();

    auto newLine = [&] {
        out += u'\n';
        out += QString(depth * indentWidth, u' ');
        ++lineBreaks;
    };
    auto nextSignificant = [&](qsizetype from) {
        while (from < n && json[from].isSpace()) {
            ++from;
        }
        return from;
    };

    qsizetype i = nextSignificant(0);
    while (i < n) {
        const QChar c = json[i];
        if (c == u'"') {
            // A string goes out untouched, escapes and all.
            qsizetype end = i + 1;
            while (end < n && json[end] != u'"') {
                end += json[end] == u'\\' ? 2 : 1;
            }
            end = qMin(end + 1, n);
            out += json.mid(i, end - i);
            i = end;
        } else if (c == u'{' || c == u'[') {
            const qsizetype next = nextSignificant(i + 1);
            if (next < n && json[next] == (c == u'{' ? u'}' : u']')) {
                out += c;
                out += json[next]; // an empty object or array stays on its line
                i = next + 1;
            } else {
                out += c;
                ++depth;
                newLine();
                i = next;
            }
        } else if (c == u'}' || c == u']') {
            depth = qMax(0, depth - 1);
            newLine();
            out += c;
            ++i;
        } else if (c == u',') {
            out += c;
            newLine();
            i = nextSignificant(i + 1);
        } else if (c == u':') {
            out += ": "_L1;
            i = nextSignificant(i + 1);
        } else if (c.isSpace()) {
            i = nextSignificant(i);
        } else {
            // A number, true, false or null.
            qsizetype end = i + 1;
            while (end < n && !json[end].isSpace() && json[end] != u',' && json[end] != u'}' && json[end] != u']' && json[end] != u':'
                   && json[end] != u'"' && json[end] != u'{' && json[end] != u'[') {
                ++end;
            }
            out += json.mid(i, end - i);
            i = end;
        }
    }
    return lineBreaks;
}

qsizetype firstSignificant(QStringView text)
{
    qsizetype i = 0;
    while (i < text.size() && text[i].isSpace()) {
        ++i;
    }
    return i;
}
}

QString reindentJson(QStringView json, int indentWidth)
{
    QString out;
    out.reserve(json.size() + json.size() / 2);
    reindentInto(json, qMax(0, indentWidth), out);
    return out;
}

bool isJsonLinesFile(const QString &fileName)
{
    return fileName.endsWith(".jsonl"_L1, Qt::CaseInsensitive) || fileName.endsWith(".ndjson"_L1, Qt::CaseInsensitive);
}

bool isMinifiedJson(QStringView json)
{
    // Written for people, JSON has a line break after the first bracket and short lines.
    const qsizetype start = firstSignificant(json);
    if (start >= json.size() || (json[start] != u'{' && json[start] != u'[')) {
        return false;
    }
    const qsizetype lineEnd = json.indexOf(u'\n', start);
    const qsizetype firstLine = (lineEnd < 0 ? json.size() : lineEnd) - start;
    return firstLine > 400;
}

FormattedJson formatJsonLines(const QString &text, int maxRecords, qsizetype maxCharacters, int indentWidth)
{
    FormattedJson result;
    result.text.reserve(qMin(text.size(), maxCharacters > 0 ? maxCharacters : text.size()) * 3 / 2);
    indentWidth = qMax(0, indentWidth);

    int outLine = 0;
    int sourceLine = 0;
    qsizetype pos = 0;
    const qsizetype n = text.size();
    while (pos < n) {
        qsizetype end = text.indexOf(u'\n', pos);
        if (end < 0) {
            end = n;
        }
        ++sourceLine;
        const QStringView line = QStringView(text).mid(pos, end - pos);
        const qsizetype start = firstSignificant(line);
        if (start < line.size()) {
            const bool full = (maxRecords > 0 && result.recordLines.size() >= maxRecords) || (maxCharacters > 0 && pos >= maxCharacters);
            if (full) {
                result.cut = true;
                break;
            }
            if (!result.text.isEmpty()) {
                result.text += u'\n';
                ++outLine;
            }
            result.recordLines.append(outLine);
            result.sourceLines.append(sourceLine);
            if (line[start] == u'{' || line[start] == u'[') {
                outLine += reindentInto(line, indentWidth, result.text);
            } else {
                result.text += line.trimmed();
            }
        }
        pos = end + 1;
    }
    return result;
}

}
