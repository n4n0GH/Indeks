// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "delimited.h"

#include <QAbstractTableModel>
#include <QPoint>
#include <QtQml/qqmlregistration.h>

#include <memory>

/**
 * The rows and columns of a CSV or TSV file, for the table the preview shows such files as.
 *
 * The file is read and taken apart away from the user interface; `loading` tells while that
 * goes on. Very large files are shown from their beginning, and `partial` says so.
 */
class TableModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ELEMENT
    /** The file to show. */
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    /** The name of the document, which may tell what separates the values. */
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    /** Whether the first row names the columns. Guessed when a file is loaded; can be set. */
    Q_PROPERTY(bool headerRow READ hasHeaderRow WRITE setHeaderRow NOTIFY layoutReset)
    /** Rows of data, not counting the one that names the columns. */
    Q_PROPERTY(int rows READ rows NOTIFY layoutReset)
    Q_PROPERTY(int columns READ columns NOTIFY layoutReset)
    /** Whether the file is larger than what is shown of it. */
    Q_PROPERTY(bool partial READ isPartial NOTIFY layoutReset)
public:
    enum Role {
        NumericRole = Qt::UserRole + 1,
    };

    explicit TableModel(QObject *parent = nullptr);
    ~TableModel() override;

    QString path() const
    {
        return m_path;
    }
    void setPath(const QString &path);
    QString fileName() const
    {
        return m_fileName;
    }
    void setFileName(const QString &fileName);
    bool isLoading() const
    {
        return m_loading;
    }
    bool hasHeaderRow() const
    {
        return m_headerRow;
    }
    void setHeaderRow(bool headerRow);
    int rows() const;
    int columns() const;
    bool isPartial() const
    {
        return m_partial;
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /** What the column is called: its cell in the header row, or a letter like a spreadsheet's. */
    Q_INVOKABLE QString columnName(int column) const;
    /** Whether the column holds numbers, which read best aligned to the right. */
    Q_INVOKABLE bool isNumeric(int column) const;
    /** How many characters the column wants to be wide, going by its first rows. */
    Q_INVOKABLE int widthHint(int column) const;
    /** The whole text of a cell, for copying and for the tooltip of one that is cut short. */
    Q_INVOKABLE QString cellText(int row, int column) const;
    /** Column (x) and row (y) of the first cell that holds one of @p terms; (-1, -1) if none does. */
    Q_INVOKABLE QPoint find(const QStringList &terms) const;
    /**
     * Every cell that contains @p text, whatever its case, row by row and @p limit of them
     * at most: each as a point with the column as x and the row as y.
     */
    Q_INVOKABLE QVariantList findAll(const QString &text, int limit) const;

Q_SIGNALS:
    void pathChanged();
    void fileNameChanged();
    void loadingChanged();
    /** Another file, or the same one read differently: everything about the table is new. */
    void layoutReset();

private:
    void load();
    void analyse();

    QString m_path;
    QString m_fileName;
    std::shared_ptr<const Indeks::DelimitedTable> m_table;
    QList<bool> m_numeric;
    bool m_loading = false;
    bool m_headerRow = false;
    bool m_partial = false;
    quint64 m_ticket = 0;
};
