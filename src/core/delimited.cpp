// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "delimited.h"

#include <QHash>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace Indeks
{

namespace
{
// How many rows are looked at to tell what a column holds and how wide it is.
constexpr int SampleRows = 300;
// How many lines are looked at to tell the separator.
constexpr int SniffLines = 30;
constexpr int MaxWidthHint = 60;
}

bool DelimitedTable::isDelimitedFile(const QString &fileName, const QString &mimeType)
{
    static const QRegularExpression names(u"\\.(csv|tsv|tab)$"_s, QRegularExpression::CaseInsensitiveOption);
    return names.match(fileName).hasMatch() || mimeType == "text/csv"_L1 || mimeType == "text/tab-separated-values"_L1 || mimeType == "text/x-comma-separated-values"_L1;
}

QChar DelimitedTable::guessSeparator(const QString &text, const QString &fileName)
{
    static const QRegularExpression tabbed(u"\\.(tsv|tab)$"_s, QRegularExpression::CaseInsensitiveOption);
    if (tabbed.match(fileName).hasMatch()) {
        return u'\t';
    }
    // Count each candidate in the first lines, leaving out what stands in quotes.
    const QList<QChar> candidates = {u',', u';', u'\t', u'|'};
    QList<QList<int>> counts(candidates.size());
    QList<int> current(candidates.size(), 0);
    bool quoted = false;
    bool any = false;
    int lines = 0;
    auto endLine = [&] {
        if (any) {
            for (qsizetype c = 0; c < candidates.size(); ++c) {
                counts[c].append(current.at(c));
            }
            ++lines;
        }
        current.fill(0);
        any = false;
    };
    for (qsizetype i = 0; i < text.size() && lines < SniffLines; ++i) {
        const QChar ch = text.at(i);
        if (ch == u'"') {
            quoted = !quoted;
            any = true;
        } else if (!quoted && (ch == u'\n' || ch == u'\r')) {
            endLine();
        } else {
            any = true;
            if (!quoted) {
                const qsizetype which = candidates.indexOf(ch);
                if (which >= 0) {
                    ++current[which];
                }
            }
        }
    }
    endLine();

    // The best is in every line the same number of times; failing that, the one that is in
    // the most lines at all.
    QChar best = u',';
    double bestScore = 0;
    for (qsizetype c = 0; c < candidates.size(); ++c) {
        const QList<int> &perLine = counts.at(c);
        if (perLine.isEmpty()) {
            continue;
        }
        QHash<int, int> frequency;
        int present = 0;
        for (int count : perLine) {
            if (count > 0) {
                ++frequency[count];
                ++present;
            }
        }
        int steadiest = 0;
        for (int times : std::as_const(frequency)) {
            steadiest = qMax(steadiest, times);
        }
        const double score = double(steadiest) / perLine.size() + 0.25 * present / perLine.size();
        if (score > bestScore) {
            bestScore = score;
            best = candidates.at(c);
        }
    }
    return best;
}

DelimitedTable::DelimitedTable(const QString &text, QChar separator)
    : m_text(text)
    , m_separator(separator)
{
    const qsizetype size = m_text.size();
    const QChar *data = m_text.constData();
    qsizetype i = 0;
    // A byte order mark is not part of the first cell.
    if (size > 0 && data[0] == QChar(0xFEFF)) {
        i = 1;
    }
    m_cells.reserve(size / 8);
    m_rows.reserve(size / 64);

    bool rowOpen = false;
    qsizetype rowStart = 0;
    auto beginRow = [&] {
        if (!rowOpen) {
            rowOpen = true;
            rowStart = m_cells.size();
        }
    };
    auto endRow = [&] {
        if (!rowOpen) {
            return;
        }
        rowOpen = false;
        const qsizetype cells = m_cells.size() - rowStart;
        // A line with nothing in it is no row.
        if (cells == 1 && m_cells.constLast().length == 0) {
            m_cells.removeLast();
            return;
        }
        m_rows.append(quint32(rowStart));
        m_columns = qMax(m_columns, int(cells));
    };

    while (i < size) {
        beginRow();
        Cell cell;
        if (data[i] == u'"') {
            // In quotes: up to the quote that is not doubled.
            qsizetype j = i + 1;
            bool escaped = false;
            while (j < size) {
                if (data[j] == u'"') {
                    if (j + 1 < size && data[j + 1] == u'"') {
                        escaped = true;
                        j += 2;
                        continue;
                    }
                    break;
                }
                ++j;
            }
            cell.start = quint32(i + 1);
            cell.length = quint32(j - i - 1);
            cell.escaped = escaped;
            i = qMin(size, j + 1);
            // Whatever follows the closing quote before the separator is not worth keeping.
            while (i < size && data[i] != m_separator && data[i] != u'\n' && data[i] != u'\r') {
                ++i;
            }
        } else {
            qsizetype j = i;
            while (j < size && data[j] != m_separator && data[j] != u'\n' && data[j] != u'\r') {
                ++j;
            }
            cell.start = quint32(i);
            cell.length = quint32(j - i);
            i = j;
        }
        m_cells.append(cell);

        if (i >= size) {
            break;
        }
        if (data[i] == m_separator) {
            ++i;
            if (i >= size) {
                m_cells.append(Cell{quint32(size), 0, 0}); // the line ends with a separator
            }
            continue;
        }
        // The end of the line, however it is written.
        if (data[i] == u'\r' && i + 1 < size && data[i + 1] == u'\n') {
            ++i;
        }
        ++i;
        endRow();
    }
    endRow();
    m_rows.append(quint32(m_cells.size())); // one past the last row, to measure it by
    m_cells.squeeze();
}

QString DelimitedTable::cell(int row, int column) const
{
    if (row < 0 || row >= rowCount() || column < 0) {
        return {};
    }
    const quint32 first = m_rows.at(row);
    const quint32 count = m_rows.at(row + 1) - first;
    if (quint32(column) >= count) {
        return {};
    }
    const Cell &cell = m_cells.at(first + column);
    QString text = m_text.mid(cell.start, cell.length);
    if (cell.escaped) {
        text.replace("\"\""_L1, "\""_L1);
    }
    return text;
}

bool DelimitedTable::looksNumeric(QStringView text)
{
    // Amounts as people write them: signs, digits grouped by dots, commas or spaces, a
    // currency or a percent sign beside them.
    static const QRegularExpression number(u"^[-+]?[$€£¥]?\\s?\\d[\\d.,'  ]*(?:[eE][-+]?\\d+)?\\s?(?:%|[$€£¥]|[A-Z]{3})?$"_s);
    const QStringView trimmed = text.trimmed();
    return !trimmed.isEmpty() && number.matchView(trimmed).hasMatch();
}

bool DelimitedTable::startsWithHeader() const
{
    if (rowCount() < 2) {
        return false;
    }
    const quint32 count = m_rows.at(1) - m_rows.at(0);
    for (quint32 column = 0; column < count; ++column) {
        const QString text = cell(0, int(column)).trimmed();
        if (text.isEmpty() || looksNumeric(text)) {
            return false;
        }
    }
    return count > 0;
}

bool DelimitedTable::isNumeric(int column, int firstRow) const
{
    int filled = 0;
    int numbers = 0;
    const int last = qMin(rowCount(), firstRow + SampleRows);
    for (int row = firstRow; row < last; ++row) {
        const QString text = cell(row, column);
        if (text.trimmed().isEmpty()) {
            continue;
        }
        ++filled;
        numbers += looksNumeric(text) ? 1 : 0;
    }
    return filled > 0 && numbers * 10 >= filled * 9;
}

int DelimitedTable::widthHint(int column) const
{
    int longest = 0;
    const int last = qMin(rowCount(), SampleRows);
    for (int row = 0; row < last && longest < MaxWidthHint; ++row) {
        const QString text = cell(row, column);
        const qsizetype lineEnd = text.indexOf(u'\n');
        longest = qMax(longest, int(lineEnd < 0 ? text.size() : lineEnd));
    }
    return qMin(longest, MaxWidthHint);
}

std::pair<int, int> DelimitedTable::find(const QStringList &terms, int fromRow) const
{
    if (terms.isEmpty()) {
        return {-1, -1};
    }
    for (int row = qMax(0, fromRow); row < rowCount(); ++row) {
        const quint32 first = m_rows.at(row);
        const quint32 count = m_rows.at(row + 1) - first;
        for (quint32 column = 0; column < count; ++column) {
            const Cell &cell = m_cells.at(first + column);
            const QStringView text = QStringView(m_text).mid(cell.start, cell.length);
            for (const QString &term : terms) {
                if (!term.isEmpty() && text.contains(term, Qt::CaseInsensitive)) {
                    return {row, int(column)};
                }
            }
        }
    }
    return {-1, -1};
}

QList<std::pair<int, int>> DelimitedTable::findAll(const QString &text, int fromRow, int limit) const
{
    QList<std::pair<int, int>> cells;
    if (text.isEmpty()) {
        return cells;
    }
    for (int row = qMax(0, fromRow); row < rowCount() && cells.size() < limit; ++row) {
        const quint32 first = m_rows.at(row);
        const quint32 count = m_rows.at(row + 1) - first;
        for (quint32 column = 0; column < count && cells.size() < limit; ++column) {
            const Cell &cell = m_cells.at(first + column);
            if (QStringView(m_text).mid(cell.start, cell.length).contains(text, Qt::CaseInsensitive)) {
                cells.append({row, int(column)});
            }
        }
    }
    return cells;
}
}
