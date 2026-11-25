// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "textformat.h"

#include "delimited.h"

#include <QList>
#include <QRegularExpression>
#include <QSet>
#include <QStringView>

using namespace Qt::StringLiterals;

namespace Indeks::TextFormat
{

namespace
{
// How much of a text is looked at.
constexpr int MaxLines = 400;
// A table has at least this many rows; fewer lines agree with one another by chance.
constexpr int MinTableRows = 3;
// With one separator to a line, which a sentence has as easily as a row of values, more
// rows have to agree.
constexpr int MinTableRowsOfTwo = 5;

QList<QStringView> linesOf(const QString &text)
{
    QList<QStringView> lines;
    qsizetype start = 0;
    const QStringView view(text);
    while (start <= view.size() && lines.size() < MaxLines) {
        qsizetype end = view.indexOf(u'\n', start);
        if (end < 0) {
            end = view.size();
        }
        QStringView line = view.mid(start, end - start);
        if (line.endsWith(u'\r')) {
            line.chop(1);
        }
        lines.append(line);
        start = end + 1;
    }
    return lines;
}

bool isBlank(QStringView line)
{
    return line.trimmed().isEmpty();
}

/** A line as the dividing line of a Markdown table: `|---|:--:|`. */
bool isTableRule(QStringView line)
{
    static const QRegularExpression rule(u"^\\s*\\|?\\s*:?-{3,}:?\\s*(\\|\\s*:?-{3,}:?\\s*)+\\|?\\s*$"_s);
    static const QRegularExpression single(u"^\\s*\\|\\s*:?-{3,}:?\\s*\\|\\s*$"_s);
    return rule.matchView(line).hasMatch() || single.matchView(line).hasMatch();
}

bool looksLikeMarkdown(const QList<QStringView> &lines)
{
    static const QRegularExpression heading(u"^#{1,6}[ \\t]+\\S"_s);
    static const QRegularExpression link(u"!?\\[[^\\]]+\\]\\((?:https?://|mailto:|[./#\\w])[^)\\s]*\\)"_s);
    static const QRegularExpression bold(u"(\\*\\*|__)\\S(?:.*?\\S)?\\1"_s);
    static const QRegularExpression code(u"`[^`]+`"_s);
    static const QRegularExpression task(u"^\\s{0,3}[-*+][ \\t]+\\[[ xX]\\][ \\t]+\\S"_s);
    static const QRegularExpression item(u"^\\s{0,3}(?:[-*+]|\\d{1,3}[.)])[ \\t]+\\S"_s);

    int fences = 0;
    int headings = 0;
    int headingsApart = 0;
    QSet<qsizetype> levels;
    int links = 0;
    int bolds = 0;
    int codes = 0;
    int tasks = 0;
    int items = 0;
    bool tableRule = false;
    bool fenced = false;

    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QStringView line = lines.at(i);
        const QStringView trimmed = line.trimmed();
        if (trimmed.startsWith("```"_L1) || trimmed.startsWith("~~~"_L1)) {
            ++fences;
            fenced = !fenced;
            continue;
        }
        if (fenced) {
            continue; // what is quoted as code is not written in Markdown
        }
        if (heading.matchView(line).hasMatch()) {
            ++headings;
            // A heading stands by itself. Lines that begin with the sign one after the
            // other are comments, as in a file of settings or a script.
            const bool before = i == 0 || isBlank(lines.at(i - 1));
            const bool after = i + 1 >= lines.size() || !lines.at(i + 1).startsWith(u'#');
            if (before && after) {
                ++headingsApart;
                qsizetype level = 0;
                while (level < line.size() && line.at(level) == u'#') {
                    ++level;
                }
                levels.insert(level);
            }
            continue;
        }
        if (isTableRule(line) && i > 0 && lines.at(i - 1).contains(u'|')) {
            tableRule = true;
        }
        if (task.matchView(line).hasMatch()) {
            ++tasks;
        }
        if (item.matchView(line).hasMatch()) {
            ++items;
        }
        if (link.matchView(line).hasMatch()) {
            ++links;
        }
        if (bold.matchView(line).hasMatch()) {
            ++bolds;
        }
        for (auto found = code.globalMatchView(line); found.hasNext(); found.next()) {
            ++codes;
        }
    }

    // Things nobody writes by chance.
    if (tableRule || fences >= 2) {
        return true;
    }
    // The kinds of marks that are there, apart from headings and plain lists, which prose
    // has too: a list of things with a dash before each is no Markdown yet.
    const int kinds = int(links > 0) + int(bolds > 0) + int(codes >= 2) + int(tasks > 0);
    const bool hasHeadings = headings > 0 && headingsApart == headings;
    if (hasHeadings) {
        return headingsApart >= 2 || levels.size() >= 2 || kinds >= 1 || items >= 2;
    }
    if (headings > 0) {
        return false; // comments, see above
    }
    return kinds >= 2 || links >= 2 || tasks >= 2;
}

bool looksLikeTable(const QList<QStringView> &lines)
{
    const QList<QChar> separators = {u'\t', u',', u';', u'|'};
    for (const QChar separator : separators) {
        int rows = 0;
        int width = -1;
        bool even = true;
        bool quoted = false;
        int count = 0;
        // A value in quotes may hold the separator, and line breaks: such a row goes on.
        for (const QStringView line : lines) {
            if (!quoted && isBlank(line)) {
                continue;
            }
            for (const QChar ch : line) {
                if (ch == u'"') {
                    quoted = !quoted;
                } else if (!quoted && ch == separator) {
                    ++count;
                }
            }
            if (quoted) {
                continue;
            }
            if (width < 0) {
                width = count;
            } else if (count != width) {
                even = false;
                break;
            }
            ++rows;
            count = 0;
        }
        if (!even || width < 1 || quoted) {
            continue;
        }
        // Tabs are not typed into a sentence; the others are.
        const int needed = separator == u'\t' || width >= 2 ? MinTableRows : MinTableRowsOfTwo;
        if (rows >= needed) {
            return true;
        }
    }
    return false;
}
}

Guess guess(const QString &text)
{
    QList<QStringView> lines = linesOf(text);
    // The last of them may be cut short where the text is only the beginning of a file.
    if (lines.size() == MaxLines || (lines.size() > MinTableRowsOfTwo && !text.endsWith(u'\n'))) {
        lines.removeLast();
    }
    while (!lines.isEmpty() && isBlank(lines.constLast())) {
        lines.removeLast();
    }
    if (lines.isEmpty()) {
        return Guess::Plain;
    }
    if (looksLikeMarkdown(lines)) {
        return Guess::Markdown;
    }
    // Also rows with bars between spaces, which may hold bars themselves and may go on in
    // lines that are set in: a list of things found, with the text of each below it.
    return looksLikeTable(lines) || DelimitedTable::looksLikeSpacedBars(text) ? Guess::Table : Guess::Plain;
}

}
