// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tablemodel.h"

#include "extractor.h"

#include <KLocalizedString>

#include <QCoreApplication>
#include <QFileInfo>
#include <QPointer>
#include <QThreadPool>

using namespace Qt::StringLiterals;
using Indeks::DelimitedTable;

namespace
{
// What is read of a file. More than this is rare, takes its time and a good deal of memory.
constexpr qint64 MaxTableBytes = 48LL * 1024 * 1024;
// Cells are shown on one line; no need to hand the view more than fits on any screen.
constexpr int MaxShownCharacters = 500;
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
    m_numeric.clear();
    if (!m_table) {
        return;
    }
    for (int column = 0; column < m_table->columnCount(); ++column) {
        m_numeric.append(m_table->isNumeric(column, m_headerRow ? 1 : 0));
    }
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
    return m_table ? m_table->columnCount() : 0;
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
    return m_table ? m_table->cell(row + (m_headerRow ? 1 : 0), column) : QString();
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
    if (m_headerRow && m_table) {
        const QString name = m_table->cell(0, column).simplified();
        if (!name.isEmpty()) {
            return name;
        }
    }
    // A, B, …, Z, AA, AB, … as spreadsheets call them.
    QString letters;
    for (int rest = column; rest >= 0; rest = rest / 26 - 1) {
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
    return orientation == Qt::Horizontal ? QVariant(columnName(section)) : QVariant(section + 1 + (m_headerRow ? 1 : 0));
}

QHash<int, QByteArray> TableModel::roleNames() const
{
    return {{Qt::DisplayRole, "display"}, {NumericRole, "numeric"}};
}

bool TableModel::isNumeric(int column) const
{
    return m_numeric.value(column, false);
}

int TableModel::widthHint(int column) const
{
    if (!m_table) {
        return 0;
    }
    return qMax(m_table->widthHint(column), int(columnName(column).size()));
}

QPoint TableModel::find(const QStringList &terms) const
{
    if (!m_table) {
        return {-1, -1};
    }
    const int first = m_headerRow ? 1 : 0;
    const auto [row, column] = m_table->find(terms, first);
    return row < 0 ? QPoint(-1, -1) : QPoint(column, row - first);
}

QVariantList TableModel::findAll(const QString &text, int limit) const
{
    QVariantList cells;
    if (!m_table) {
        return cells;
    }
    const int first = m_headerRow ? 1 : 0;
    const auto found = m_table->findAll(text, first, limit);
    cells.reserve(found.size());
    for (const auto &[row, column] : found) {
        cells.append(QPoint(column, row - first));
    }
    return cells;
}

#include "moc_tablemodel.cpp"
