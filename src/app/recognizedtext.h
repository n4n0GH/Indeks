// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "ocr.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

/**
 * The text that was recognised on a scan, for the viewer that shows the scan: to select
 * with the mouse, to copy, and to search.
 *
 * A scan is a picture; what makes its text selectable is that the index knows where each
 * recognised word stands (Library::recognizedLayout()). This does on those words what
 * PdfText does on the text of a PDF, by the same names, so that a viewer treats the two
 * alike, and what Qt's PdfSearchModel does for finding.
 *
 * Places are shares of the width and the height of the page, from its top left corner (0
 * to 1), whatever size the page is shown at. A picture is page 0. One selection at a time,
 * on one page, and always whole words: where a word begins and ends is known, where its
 * letters do is not.
 */
class RecognizedText : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qint64 documentId READ documentId WRITE setDocumentId NOTIFY documentIdChanged)
    /**
     * Pages that have text of their own, which this then leaves alone: in a PDF of which
     * some pages are scans and others are not, the others are the PDF's to select and find.
     */
    Q_PROPERTY(QList<int> ownTextPages READ ownTextPages WRITE setOwnTextPages NOTIFY handledChanged)
    /** Whether there are words to select anywhere in the document. */
    Q_PROPERTY(bool available READ isAvailable NOTIFY handledChanged)
    /** How many pages the words are known of, whoever handles them. */
    Q_PROPERTY(int pageCount READ pageCount NOTIFY layoutChanged)
    /** Goes up when the words to handle change, for what is asked through functions. */
    Q_PROPERTY(int revision READ revision NOTIFY handledChanged)

    /** The page the selection is on, or -1 if nothing is selected. */
    Q_PROPERTY(int page READ page NOTIFY selectionChanged)
    Q_PROPERTY(QString text READ text NOTIFY selectionChanged)
    /** What is selected as shapes on the page, for PathMultiline. */
    Q_PROPERTY(QList<QPolygonF> geometry READ geometry NOTIFY selectionChanged)
    Q_PROPERTY(bool empty READ isEmpty NOTIFY selectionChanged)

    /** What to find; upper and lower case are the same. */
    Q_PROPERTY(QString findText READ findText WRITE setFindText NOTIFY findChanged)
    Q_PROPERTY(int findCount READ findCount NOTIFY findChanged)
    /** The page of the hit that was gone to last, and its shapes; -1 and none without one. */
    Q_PROPERTY(int currentPage READ currentPage NOTIFY currentChanged)
    Q_PROPERTY(QList<QPolygonF> currentMarks READ currentMarks NOTIFY currentChanged)
public:
    explicit RecognizedText(QObject *parent = nullptr);

    qint64 documentId() const
    {
        return m_documentId;
    }
    void setDocumentId(qint64 id);
    QList<int> ownTextPages() const
    {
        return m_ownTextPages;
    }
    void setOwnTextPages(const QList<int> &pages);
    bool isAvailable() const;
    int pageCount() const
    {
        return int(m_pages.size());
    }
    int revision() const
    {
        return m_revision;
    }

    int page() const
    {
        return m_page;
    }
    QString text() const
    {
        return m_text;
    }
    QList<QPolygonF> geometry() const
    {
        return m_geometry;
    }
    bool isEmpty() const
    {
        return m_text.isEmpty();
    }

    QString findText() const
    {
        return m_findText;
    }
    void setFindText(const QString &text);
    int findCount() const
    {
        return int(m_hits.size());
    }
    int currentPage() const
    {
        return m_current >= 0 && m_current < m_hits.size() ? m_hits.at(m_current).page : -1;
    }
    QList<QPolygonF> currentMarks() const;

    /** Whether there are recognised words on a page that this is to handle. */
    Q_INVOKABLE bool hasWords(int page) const;
    /** Selects the words from the one of two places to the other, in whichever order. */
    Q_INVOKABLE void selectBetween(int page, const QPointF &from, const QPointF &to);
    /** Selects the word at, or nearest to, a place. */
    Q_INVOKABLE void selectWordAt(int page, const QPointF &point);
    /** Selects all the words of a page. */
    Q_INVOKABLE void selectPage(int page);
    Q_INVOKABLE void clear();

    /**
     * What is to be said about a hit: `page`, the text `before` it, the `match` as it reads
     * on the page and the text `after` it, and `top`, how far down the page it stands.
     */
    Q_INVOKABLE QVariantMap hit(int index) const;
    /** The shapes of all hits on a page. */
    Q_INVOKABLE QList<QPolygonF> marks(int page) const;
    /** Makes a hit the one that was gone to; -1 for none. */
    Q_INVOKABLE void setCurrent(int index);

Q_SIGNALS:
    void documentIdChanged();
    /** Other words were read from the index. */
    void layoutChanged();
    /** Which words this handles has changed: other words, or other pages left alone. */
    void handledChanged();
    void selectionChanged();
    void findChanged();
    void currentChanged();

private:
    struct Line {
        QRectF rect;
        int first = 0; ///< its first word, counted through the page
        int last = 0;
    };
    struct Hit {
        int page = 0;
        QList<QRectF> rects;
        QString before;
        QString match;
        QString after;
    };
    void load();
    const QList<Line> &lines(int page);
    /** The word at a place, or the nearest one; -1 on a page without words. */
    int nearest(int page, const QPointF &point);
    void take(int page, int first, int last);
    void find();
    static QList<QPolygonF> shapes(const QList<QRectF> &rects);

    qint64 m_documentId = 0;
    QList<Indeks::Ocr::Page> m_pages;
    QList<int> m_ownTextPages;
    QSet<int> m_skipped;
    QHash<int, QList<Line>> m_lines;
    int m_revision = 0;

    int m_page = -1;
    QString m_text;
    QList<QPolygonF> m_geometry;

    QString m_findText;
    QList<Hit> m_hits;
    int m_current = -1;
};
