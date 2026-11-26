// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QHash>
#include <QObject>
#include <QPdfDocument>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QUrl>
#include <qqmlintegration.h>

/**
 * Text of a PDF that is selected with the mouse, for the viewer to mark and to copy.
 *
 * Qt's own PdfSelection only selects between two points that both lie on a letter: a drag
 * that starts in the margin or ends below a paragraph selects nothing at all. This one takes
 * any two points of a page and goes by the text that is nearest to each, the way selecting
 * works everywhere else.
 *
 * Places are in points (1/72 inch) from the top left corner of the page, which is what the
 * PDF goes by. One selection at a time, on one page.
 */
class PdfText : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    /** The PDF file; the same one the viewer shows. */
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    /** The page the selection is on, or -1 if nothing is selected. */
    Q_PROPERTY(int page READ page NOTIFY selectionChanged)
    Q_PROPERTY(QString text READ text NOTIFY selectionChanged)
    /** What is selected as shapes on the page, for PathMultiline. */
    Q_PROPERTY(QList<QPolygonF> geometry READ geometry NOTIFY selectionChanged)
    Q_PROPERTY(bool empty READ isEmpty NOTIFY selectionChanged)
public:
    explicit PdfText(QObject *parent = nullptr);

    QUrl source() const
    {
        return m_source;
    }
    void setSource(const QUrl &source);
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

    /** Selects the text from the one of two places to the other, in whichever order. */
    Q_INVOKABLE void selectBetween(int page, const QPointF &from, const QPointF &to);
    /** Selects the word at, or nearest to, a place. */
    Q_INVOKABLE void selectWordAt(int page, const QPointF &point);
    /** Selects all the text of a page. */
    Q_INVOKABLE void selectPage(int page);
    /**
     * Which of the first @p count pages have text of their own. The others are pictures of
     * text, scans, whose text is the recognised one; see RecognizedText.
     */
    Q_INVOKABLE QList<int> pagesWithText(int count);
    Q_INVOKABLE void clear();

Q_SIGNALS:
    void sourceChanged();
    void selectionChanged();

private:
    /** A place on a line of text, and which of the page's lines that is; -1 if there is none. */
    struct Hit {
        int line = -1;
        QPointF point;
        QRectF rect;
    };
    const QList<QRectF> &lines(int page);
    Hit nearest(int page, const QPointF &point);
    void take(int page, const QPdfSelection &selection);

    QUrl m_source;
    QPdfDocument m_document;
    /** Where the lines of text of the pages looked at so far stand, in reading order. */
    QHash<int, QList<QRectF>> m_lines;
    int m_page = -1;
    QString m_text;
    QList<QPolygonF> m_geometry;
};
