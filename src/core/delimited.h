// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QChar>
#include <QList>
#include <QString>
#include <QStringList>

namespace Indeks
{

/**
 * A file of values in rows and columns, separated by commas, semicolons, tabs or bars (CSV,
 * TSV), taken apart for showing as a table.
 *
 * Such files get large, so the text is kept in one piece and the table only remembers where
 * each cell is in it; a cell is cut out when it is asked for. Read in the usual lenient way:
 * a cell in quotes may hold separators, line breaks and doubled quotes; rows may differ in
 * length; lines without anything in them are left out.
 *
 * Bars are also written with room around them, by people and by programs that list what
 * they found (`who | when | what`), and such a list may go on below each row with a line
 * of text that is set in: see Dialect.
 */
class DelimitedTable
{
public:
    /** How a file writes its rows, beyond which sign separates the values. */
    struct Dialect {
        QChar separator = u',';
        /**
         * The separator only separates where it stands between spaces (`a | b`), so that a
         * value may hold the sign itself (`a|b`). Values are taken without the spaces around
         * them, and quotes are part of them: such values are text, which may well begin with
         * one.
         */
        bool spaced = false;
        /**
         * A line that is set in belongs to the row above it, as one more value at its end:
         * the text a row is about, below the row.
         */
        bool continued = false;
    };

    DelimitedTable() = default;
    /** Takes @p text apart at @p separator. */
    DelimitedTable(const QString &text, QChar separator);
    DelimitedTable(const QString &text, const Dialect &dialect);

    /**
     * How a file writes its rows, found by looking at its first lines: the separator as
     * guessSeparator() finds it, and whether bars stand between spaces and rows go on in
     * lines that are set in.
     */
    static Dialect guessDialect(const QString &text, const QString &fileName);
    /**
     * Whether @p text is rows of values with bars between spaces: every line that is not
     * set in has the same number of them, and there are enough such lines for that to be
     * no accident. Lines that are set in go on with the row above.
     */
    static bool looksLikeSpacedBars(const QString &text);

    /**
     * The separator a file uses, found by looking at its first lines: the one that occurs in
     * every line, and the same number of times. A tab for files that say so by their name.
     */
    static QChar guessSeparator(const QString &text, const QString &fileName);
    /** Whether a file of this name or type is one of rows and columns. */
    static bool isDelimitedFile(const QString &fileName, const QString &mimeType);

    QChar separator() const
    {
        return m_dialect.separator;
    }
    Dialect dialect() const
    {
        return m_dialect;
    }
    int rowCount() const
    {
        return qMax(0, int(m_rows.size()) - 1); // the list ends with where the cells end
    }
    /** Cells of the longest row. */
    int columnCount() const
    {
        return m_columns;
    }
    /** The cell as it reads, without its quotes; empty where the row has no such cell. */
    QString cell(int row, int column) const;

    /**
     * Whether the first row looks like the names of the columns rather than data: every
     * cell has something in it and none of them is a number.
     */
    bool startsWithHeader() const;
    /** Whether what column @p column holds are numbers, going by the rows from @p firstRow on. */
    bool isNumeric(int column, int firstRow) const;
    /** The longest cell of a column among the first rows, in characters; for a first width. */
    int widthHint(int column) const;
    /** The first cell at or after row @p fromRow that contains one of @p terms; {-1, -1} if none does. */
    std::pair<int, int> find(const QStringList &terms, int fromRow = 0) const;
    /**
     * Every cell from row @p fromRow on that contains @p text, whatever its case, row by
     * row; @p limit of them at most.
     */
    QList<std::pair<int, int>> findAll(const QString &text, int fromRow, int limit) const;

    static bool looksNumeric(QStringView text);

private:
    struct Cell {
        quint32 start = 0;
        quint32 length : 31 = 0;
        quint32 escaped : 1 = 0; ///< holds doubled quotes that stand for one
    };

    QString m_text;
    Dialect m_dialect;
    QList<Cell> m_cells;
    QList<quint32> m_rows; ///< where each row begins in m_cells, and at last where they end
    int m_columns = 0;
};

}
