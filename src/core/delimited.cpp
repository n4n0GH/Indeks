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
// How many lines are looked at to tell whether a text is rows with bars between spaces.
constexpr int LookLines = 400;
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

namespace
{
bool isRoom(QChar ch)
{
    return ch == u' ' || ch == u'\t';
}

// What the lines of a text say about bars between spaces.
struct BarLines {
    int rows = 0; ///< lines that are not set in
    int continuations = 0; ///< lines that are
    int width = -1; ///< bars between spaces in a row; the same in all of them if `even`
    bool even = true;
    bool allSpaced = true; ///< no bar in a row stands without room around it
    bool beginsWithRow = true;
};

BarLines barLines(const QString &text, int maxLines)
{
    BarLines found;
    const qsizetype size = text.size();
    qsizetype start = 0;
    int lines = 0;
    while (start < size && lines < maxLines) {
        qsizetype end = text.indexOf(u'\n', start);
        const bool whole = end >= 0;
        if (!whole) {
            end = size;
        }
        const QStringView line = QStringView(text).mid(start, end - start);
        start = end + 1;
        if (line.trimmed().isEmpty()) {
            continue;
        }
        // The last line may be cut short where the text is only the beginning of a file.
        if (!whole && lines > 5) {
            break;
        }
        ++lines;
        if (isRoom(line.at(0))) {
            ++found.continuations;
            found.beginsWithRow = found.beginsWithRow && found.rows > 0;
            continue;
        }
        int spaced = 0;
        for (qsizetype i = 0; i < line.size(); ++i) {
            if (line.at(i) != u'|') {
                continue;
            }
            if (i > 0 && isRoom(line.at(i - 1)) && i + 1 < line.size() && isRoom(line.at(i + 1))) {
                ++spaced;
            } else {
                found.allSpaced = false;
            }
        }
        ++found.rows;
        if (found.width < 0) {
            found.width = spaced;
        } else if (spaced != found.width) {
            found.even = false;
        }
    }
    return found;
}

// As many rows as it takes for their agreeing to be no accident: more where there is
// only one bar to a line.
bool enoughRows(const BarLines &lines)
{
    return lines.even && lines.beginsWithRow && lines.width >= 1 && lines.rows >= (lines.width >= 2 ? 3 : 5);
}
}

bool DelimitedTable::looksLikeSpacedBars(const QString &text)
{
    return enoughRows(barLines(text, LookLines));
}

DelimitedTable::Dialect DelimitedTable::guessDialect(const QString &text, const QString &fileName)
{
    Dialect dialect;
    dialect.separator = guessSeparator(text, fileName);
    const BarLines lines = barLines(text, LookLines);
    if (enoughRows(lines) && lines.continuations > 0 && dialect.separator != u'\t') {
        // Rows with text below them. Whatever sign that text is full of, it is not the
        // separator.
        return {u'|', true, true};
    }
    if (dialect.separator == u'|') {
        // Bars between spaces, the same number in every row: then a bar without room is
        // part of a value. In a file too short to tell, they all have to have their room.
        dialect.spaced = lines.continuations == 0 && lines.width >= 1 && lines.even && (lines.allSpaced || enoughRows(lines));
    }
    return dialect;
}

DelimitedTable::DelimitedTable(const QString &text, QChar separator)
    : DelimitedTable(text, Dialect{separator, false, false})
{
}

DelimitedTable::DelimitedTable(const QString &text, const Dialect &dialect)
    : m_text(text)
    , m_dialect(dialect)
{
    const qsizetype size = m_text.size();
    const QChar *data = m_text.constData();
    qsizetype i = 0;
    // A byte order mark is not part of the first cell.
    if (size > 0 && data[0] == QChar(0xFEFF)) {
        i = 1;
    }
    const QChar separator = dialect.separator;
    const bool spaced = dialect.spaced;
    const auto separates = [&](qsizetype at) {
        return data[at] == separator && (!spaced || (at > 0 && isRoom(data[at - 1]) && at + 1 < size && isRoom(data[at + 1])));
    };
    const auto lineEnds = [&](qsizetype at) {
        return data[at] == u'\n' || data[at] == u'\r';
    };
    // Past the end of the line at @p at, however it is written.
    const auto nextLine = [&](qsizetype at) {
        if (at < size && data[at] == u'\r' && at + 1 < size && data[at + 1] == u'\n') {
            ++at;
        }
        return qMin(size, at + 1);
    };
    // The text below a row stands in the column after the last one the rows have.
    const int width = dialect.continued ? qMax(0, barLines(text, LookLines).width) + 1 : 0;
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
        if (dialect.continued && !rowOpen && isRoom(data[i])) {
            // A line that is set in: it goes on with the row above, and so do the lines
            // that follow it and are set in too.
            qsizetype from = i;
            while (from < size && isRoom(data[from])) {
                ++from;
            }
            if (from >= size || lineEnds(from)) {
                i = nextLine(from); // nothing in it
                continue;
            }
            qsizetype end = from;
            for (;;) {
                while (end < size && !lineEnds(end)) {
                    ++end;
                }
                i = nextLine(end);
                qsizetype next = i;
                while (next < size && isRoom(data[next])) {
                    ++next;
                }
                if (next == i || next >= size || lineEnds(next)) {
                    break;
                }
                end = next;
            }
            while (end > from && isRoom(data[end - 1])) {
                --end;
            }
            if (m_rows.isEmpty()) {
                m_rows.append(quint32(m_cells.size())); // text with no row above it
            }
            const qsizetype rowBegin = m_rows.constLast();
            while (m_cells.size() - rowBegin < width) {
                m_cells.append(Cell{quint32(from), 0, 0});
            }
            m_cells.append(Cell{quint32(from), quint32(end - from), 0});
            m_columns = qMax(m_columns, int(m_cells.size() - rowBegin));
            continue;
        }
        beginRow();
        Cell cell;
        if (!spaced && data[i] == u'"') {
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
            while (i < size && !separates(i) && !lineEnds(i)) {
                ++i;
            }
        } else {
            qsizetype j = i;
            while (j < size && !separates(j) && !lineEnds(j)) {
                ++j;
            }
            qsizetype from = i;
            qsizetype to = j;
            if (spaced) {
                // Without the room that is part of the separator.
                while (from < to && isRoom(data[from])) {
                    ++from;
                }
                while (to > from && isRoom(data[to - 1])) {
                    --to;
                }
            }
            cell.start = quint32(from);
            cell.length = quint32(to - from);
            i = j;
        }
        m_cells.append(cell);

        if (i >= size) {
            break;
        }
        if (separates(i)) {
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

std::optional<double> DelimitedTable::number(QStringView text)
{
    if (!looksNumeric(text)) {
        return std::nullopt;
    }
    // The digits, the signs between them and what stands for a power of ten.
    QString digits;
    bool negative = false;
    for (const QChar ch : text.trimmed()) {
        if (ch.isDigit() || ch == u'.' || ch == u',') {
            digits.append(ch);
        } else if (ch == u'-' && digits.isEmpty()) {
            negative = true;
        } else if ((ch == u'e' || ch == u'E') && !digits.isEmpty()) {
            digits.append(u'e');
        } else if ((ch == u'-' || ch == u'+') && digits.endsWith(u'e')) {
            digits.append(ch);
        }
    }
    const qsizetype power = digits.indexOf(u'e');
    QString mantissa = power < 0 ? digits : digits.left(power);
    // Which of the two signs is before the fraction: the one that comes last, if both are
    // there. One alone that has exactly three digits behind it, or that is there more than
    // once, groups thousands.
    const qsizetype comma = mantissa.lastIndexOf(u',');
    const qsizetype point = mantissa.lastIndexOf(u'.');
    QChar fraction;
    if (comma >= 0 && point >= 0) {
        fraction = comma > point ? u',' : u'.';
    } else if (comma >= 0 || point >= 0) {
        const QChar sign = comma >= 0 ? u',' : u'.';
        const qsizetype at = qMax(comma, point);
        const bool groups = mantissa.count(sign) > 1 || (mantissa.size() - at - 1 == 3 && at > 0 && at <= 3);
        fraction = groups ? QChar() : sign;
    }
    QString plain;
    for (const QChar ch : std::as_const(mantissa)) {
        if (ch.isDigit()) {
            plain.append(ch);
        } else if (ch == fraction) {
            plain.append(u'.');
        }
    }
    if (power >= 0) {
        plain.append(digits.mid(power));
    }
    bool ok = false;
    const double value = plain.toDouble(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return negative ? -value : value;
}

bool DelimitedTable::startsWithHeader() const
{
    // Rows with text below them are a list of things found, not a sheet with a head.
    if (rowCount() < 2 || m_dialect.continued) {
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
