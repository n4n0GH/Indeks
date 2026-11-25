// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tablemodel.h"

#include "extractor.h"

#include <KLocalizedString>

#include <QCollator>
#include <QCoreApplication>
#include <QFileInfo>
#include <QPointer>
#include <QThreadPool>

#include <algorithm>
#include <numeric>

using namespace Qt::StringLiterals;
using Indeks::DelimitedTable;

namespace
{
// What is read of a file. More than this is rare, takes its time and a good deal of memory.
constexpr qint64 MaxTableBytes = 48LL * 1024 * 1024;
// Cells are shown on one line; no need to hand the view more than fits on any screen.
constexpr int MaxShownCharacters = 500;
// How many cells that hold a text are put in the order of a sorted table to find the first.
constexpr int MaxFoundToSort = 100000;
}

TableModel::TableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

TableModel::~TableModel() = default;

void TableModel::setPath(const QString &path)
{
    if (path == m_path) {
        return;
    }
    m_path = path;
    Q_EMIT pathChanged();
    load();
}

void TableModel::setFileName(const QString &fileName)
{
    if (fileName == m_fileName) {
        return;
    }
    m_fileName = fileName;
    Q_EMIT fileNameChanged();
    if (!m_path.isEmpty()) {
        load();
    }
}

void TableModel::load()
{
    const quint64 ticket = ++m_ticket;
    if (m_path.isEmpty()) {
        beginResetModel();
        m_table.reset();
        m_numeric.clear();
        m_partial = false;
        endResetModel();
        Q_EMIT layoutReset();
        return;
    }
    if (!m_loading) {
        m_loading = true;
        Q_EMIT loadingChanged();
    }
    // Reading and taking apart a large file takes a moment that is not the user interface's.
    QThreadPool::globalInstance()->start([self = QPointer<TableModel>(this), ticket, path = m_path, fileName = m_fileName] {
        const bool partial = QFileInfo(path).size() > MaxTableBytes;
        QString text = Indeks::readTextFile(path, MaxTableBytes);
        if (partial) {
            // Not the row the reading stopped in the middle of.
            text.truncate(qMax<qsizetype>(0, text.lastIndexOf(u'\n')));
        }
        auto table = std::make_shared<const DelimitedTable>(text, DelimitedTable::guessDialect(text, fileName));
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, ticket, table, partial] {
            if (!self || self->m_ticket != ticket) {
                return; // another file by now
            }
            self->beginResetModel();
            self->m_table = table;
            self->m_partial = partial;
            self->m_headerRow = table->startsWithHeader();
            self->analyse();
            self->endResetModel();
            self->m_loading = false;
            Q_EMIT self->loadingChanged();
            Q_EMIT self->layoutReset();
        });
    });
}

void TableModel::analyse()
{
    // Another table, or the same one with another first row of data: all of it is shown,
    // in the order of the file.
    m_numeric.clear();
    m_shown.clear();
    forgetSort();
    if (!m_table) {
        return;
    }
    for (int column = 0; column < m_table->columnCount(); ++column) {
        m_numeric.append(m_table->isNumeric(column, m_headerRow ? 1 : 0));
        m_shown.append(column);
    }
}

int TableModel::fileColumn(int column) const
{
    return m_shown.value(column, -1);
}

int TableModel::fileRow(int row) const
{
    return m_order.isEmpty() ? row : m_order.value(row, row);
}

int TableModel::hiddenColumns() const
{
    return m_table ? m_table->columnCount() - int(m_shown.size()) : 0;
}

int TableModel::sortColumn() const
{
    return int(m_shown.indexOf(m_sortedBy));
}

void TableModel::forgetSort()
{
    ++m_sortTicket; // whatever is being sorted is no longer wanted
    m_order.clear();
    m_place.clear();
    m_sortedBy = -1;
    setSorting(false);
    Q_EMIT sortChanged();
}

void TableModel::setSorting(bool sorting)
{
    if (m_sorting != sorting) {
        m_sorting = sorting;
        Q_EMIT sortingChanged();
    }
}

void TableModel::hideColumn(int column)
{
    if (column < 0 || column >= m_shown.size() || m_shown.size() < 2) {
        return;
    }
    beginResetModel();
    m_shown.removeAt(column);
    endResetModel();
    Q_EMIT layoutReset();
    Q_EMIT sortChanged(); // the column the rows are sorted by may be another one now, or none
}

void TableModel::showAllColumns()
{
    if (!m_table || hiddenColumns() == 0) {
        return;
    }
    beginResetModel();
    m_shown.resize(m_table->columnCount());
    std::iota(m_shown.begin(), m_shown.end(), 0);
    endResetModel();
    Q_EMIT layoutReset();
    Q_EMIT sortChanged();
}

void TableModel::clearSort()
{
    if (m_order.isEmpty() && !m_sorting) {
        return;
    }
    beginResetModel();
    forgetSort();
    endResetModel();
}

void TableModel::sortBy(int column, bool numeric, bool descending)
{
    const int source = fileColumn(column);
    if (!m_table || source < 0) {
        return;
    }
    const quint64 ticket = ++m_sortTicket;
    setSorting(true);
    // Reading every cell of the column and putting a long table in order takes a moment
    // that is not the user interface's.
    QThreadPool::globalInstance()->start([self = QPointer<TableModel>(this), ticket, table = m_table, source, numeric, descending, first = m_headerRow ? 1 : 0] {
        const int rows = qMax(0, table->rowCount() - first);
        QList<int> order(rows);
        std::iota(order.begin(), order.end(), 0);
        // What has nothing to be sorted by comes last, whichever way the rest goes.
        QList<bool> blank(rows);
        if (numeric) {
            QList<double> keys(rows);
            for (int row = 0; row < rows; ++row) {
                const std::optional<double> value = DelimitedTable::number(table->cell(row + first, source));
                blank[row] = !value.has_value();
                keys[row] = value.value_or(0);
            }
            std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
                if (blank.at(a) != blank.at(b)) {
                    return blank.at(b);
                }
                return !blank.at(a) && (descending ? keys.at(a) > keys.at(b) : keys.at(a) < keys.at(b));
            });
        } else {
            // As a dictionary of the user's language has it, whatever the case, and with
            // the numbers in names counted, not spelt.
            QCollator collator;
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            collator.setNumericMode(true);
            QList<QCollatorSortKey> keys;
            keys.reserve(rows);
            for (int row = 0; row < rows; ++row) {
                const QString text = table->cell(row + first, source).trimmed();
                blank[row] = text.isEmpty();
                keys.append(collator.sortKey(text));
            }
            std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
                if (blank.at(a) != blank.at(b)) {
                    return blank.at(b);
                }
                const int compared = keys.at(a).compare(keys.at(b));
                return !blank.at(a) && (descending ? compared > 0 : compared < 0);
            });
        }
        QList<int> place(rows);
        for (int shown = 0; shown < rows; ++shown) {
            place[order.at(shown)] = shown;
        }
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, ticket, table, order, place, source, numeric, descending] {
            if (!self || self->m_sortTicket != ticket || self->m_table != table) {
                return; // another table by now, or another order
            }
            self->beginResetModel();
            self->m_order = order;
            self->m_place = place;
            self->m_sortedBy = source;
            self->m_sortNumeric = numeric;
            self->m_sortDescending = descending;
            self->endResetModel();
            self->setSorting(false);
            Q_EMIT self->sortChanged();
        });
    });
}

void TableModel::setHeaderRow(bool headerRow)
{
    if (headerRow == m_headerRow) {
        return;
    }
    beginResetModel();
    m_headerRow = headerRow;
    analyse();
    endResetModel();
    Q_EMIT layoutReset();
}

int TableModel::rows() const
{
    return m_table ? qMax(0, m_table->rowCount() - (m_headerRow ? 1 : 0)) : 0;
}

int TableModel::columns() const
{
    return m_table ? int(m_shown.size()) : 0;
}

int TableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : rows();
}

int TableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : columns();
}

QString TableModel::cellText(int row, int column) const
{
    const int source = fileColumn(column);
    return m_table && source >= 0 ? m_table->cell(fileRow(row) + (m_headerRow ? 1 : 0), source) : QString();
}

QVariant TableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !m_table) {
        return {};
    }
    switch (role) {
    case Qt::DisplayRole: {
        // On one line, whatever the cell holds.
        QString text = cellText(index.row(), index.column()).left(MaxShownCharacters);
        text.replace(u'\n', u'⏎').replace(u'\r', QString()).replace(u'\t', u' ');
        return text;
    }
    case NumericRole:
        return isNumeric(index.column());
    }
    return {};
}

QString TableModel::columnName(int column) const
{
    // By the place the column has in the file, also while others before it are hidden.
    const int source = fileColumn(column);
    if (m_headerRow && m_table && source >= 0) {
        const QString name = m_table->cell(0, source).simplified();
        if (!name.isEmpty()) {
            return name;
        }
    }
    // A, B, …, Z, AA, AB, … as spreadsheets call them.
    QString letters;
    for (int rest = source; rest >= 0; rest = rest / 26 - 1) {
        letters.prepend(QChar(u'A' + rest % 26));
    }
    return letters;
}

QVariant TableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return {};
    }
    // Rows are counted as the file counts its lines of values, the header row being the first.
    // A row keeps its number wherever it is sorted to.
    return orientation == Qt::Horizontal ? QVariant(columnName(section)) : QVariant(fileRow(section) + 1 + (m_headerRow ? 1 : 0));
}

QHash<int, QByteArray> TableModel::roleNames() const
{
    return {{Qt::DisplayRole, "display"}, {NumericRole, "numeric"}};
}

bool TableModel::isNumeric(int column) const
{
    return m_numeric.value(fileColumn(column), false);
}

int TableModel::widthHint(int column) const
{
    if (!m_table) {
        return 0;
    }
    const int source = fileColumn(column);
    return source < 0 ? 0 : qMax(m_table->widthHint(source), int(columnName(column).size()));
}

QPoint TableModel::find(const QStringList &terms) const
{
    if (!m_table) {
        return {-1, -1};
    }
    const int first = m_headerRow ? 1 : 0;
    const auto [row, column] = m_table->find(terms, first);
    if (row < 0) {
        return {-1, -1};
    }
    // Where that cell is shown; nowhere if its column is hidden.
    const int shown = int(m_shown.indexOf(column));
    return shown < 0 ? QPoint(-1, -1) : QPoint(shown, m_place.isEmpty() ? row - first : m_place.value(row - first, row - first));
}

QVariantList TableModel::findAll(const QString &text, int limit) const
{
    QVariantList cells;
    if (!m_table) {
        return cells;
    }
    const int first = m_headerRow ? 1 : 0;
    const bool asInFile = m_order.isEmpty() && hiddenColumns() == 0;
    // Found in the order of the file. Where the table is shown otherwise, more are asked
    // for than wanted: some are in hidden columns, and the first of them as they are shown
    // may be far down in the file.
    const auto found = m_table->findAll(text, first, asInFile ? limit : qMax(limit, MaxFoundToSort));
    QList<QPoint> places;
    places.reserve(found.size());
    for (const auto &[row, column] : found) {
        const int shown = int(m_shown.indexOf(column));
        if (shown >= 0) {
            places.append(QPoint(shown, m_place.isEmpty() ? row - first : m_place.value(row - first, row - first)));
        }
    }
    if (!asInFile) {
        std::sort(places.begin(), places.end(), [](const QPoint &a, const QPoint &b) {
            return a.y() != b.y() ? a.y() < b.y() : a.x() < b.x();
        });
        if (places.size() > limit) {
            places.resize(limit);
        }
    }
    cells.reserve(places.size());
    for (const QPoint &place : std::as_const(places)) {
        cells.append(place);
    }
    return cells;
}

#include "moc_tablemodel.cpp"
