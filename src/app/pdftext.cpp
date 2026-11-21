// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "pdftext.h"

#include <QPdfSelection>

using namespace Qt::StringLiterals;

PdfText::PdfText(QObject *parent)
    : QObject(parent)
{
}

void PdfText::setSource(const QUrl &source)
{
    if (m_source == source) {
        return;
    }
    m_source = source;
    m_document.close();
    m_lines.clear();
    if (source.isLocalFile()) {
        m_document.load(source.toLocalFile());
    }
    clear();
    Q_EMIT sourceChanged();
}

const QList<QRectF> &PdfText::lines(int page)
{
    auto known = m_lines.find(page);
    if (known == m_lines.end()) {
        QList<QRectF> rects;
        if (page >= 0 && page < m_document.pageCount()) {
            const QList<QPolygonF> bounds = m_document.getAllText(page).bounds();
            rects.reserve(bounds.size());
            for (const QPolygonF &polygon : bounds) {
                const QRectF rect = polygon.boundingRect();
                if (rect.width() > 0 && rect.height() > 0) {
                    rects.append(rect);
                }
            }
        }
        known = m_lines.insert(page, rects);
    }
    return *known;
}

PdfText::Hit PdfText::nearest(int page, const QPointF &point)
{
    // The line at the height of the place, or the one nearest to that height; of several at
    // one height (columns, or a line set in two fonts) the one that is nearest sideways.
    const QList<QRectF> &rects = lines(page);
    Hit hit;
    qreal bestAbove = 0;
    qreal bestBeside = 0;
    for (int i = 0; i < rects.size(); ++i) {
        const QRectF &rect = rects.at(i);
        const qreal above = point.y() < rect.top() ? rect.top() - point.y() : point.y() > rect.bottom() ? point.y() - rect.bottom() : 0;
        const qreal beside = point.x() < rect.left() ? rect.left() - point.x() : point.x() > rect.right() ? point.x() - rect.right() : 0;
        if (hit.line < 0 || above < bestAbove || (above == bestAbove && beside < bestBeside)) {
            hit.line = i;
            hit.rect = rect;
            bestAbove = above;
            bestBeside = beside;
        }
    }
    if (hit.line >= 0) {
        // On the line itself. A place beyond its end goes to the very end of it, not short
        // of that: the last letter may be a full stop, which is small and sits low, and a
        // place just inside the line would be taken for the letter before it.
        hit.point = QPointF(qBound(hit.rect.left(), point.x(), hit.rect.right()), hit.rect.center().y());
    }
    return hit;
}

void PdfText::take(int page, const QPdfSelection &selection)
{
    if (!selection.isValid() || selection.text().isEmpty()) {
        clear();
        return;
    }
    m_page = page;
    // Lines end the way the PDF library reports them; a clipboard wants plain line feeds.
    m_text = selection.text();
    m_text.replace("\r\n"_L1, "\n"_L1);
    m_geometry = selection.bounds();
    Q_EMIT selectionChanged();
}

void PdfText::selectBetween(int page, const QPointF &from, const QPointF &to)
{
    Hit first = nearest(page, from);
    Hit last = nearest(page, to);
    if (first.line < 0 || last.line < 0) {
        clear();
        return;
    }
    // From the earlier to the later place in the text, whichever way the mouse went.
    if (last.line < first.line || (last.line == first.line && last.point.x() < first.point.x())) {
        std::swap(first, last);
    }
    take(page, m_document.getSelection(page, first.point, last.point));
}

void PdfText::selectWordAt(int page, const QPointF &point)
{
    const Hit hit = nearest(page, point);
    if (hit.line < 0) {
        clear();
        return;
    }
    // Which letter it is, counted through the text of the page: the last one of what is
    // selected from the start of its line up to it.
    const QPointF lineStart(hit.rect.left(), hit.point.y());
    const QPdfSelection upTo = m_document.getSelection(page, lineStart, hit.point);
    const QString all = m_document.getAllText(page).text();
    int index = upTo.isValid() ? upTo.endIndex() - 1 : m_document.getSelection(page, hit.point, hit.point + QPointF(2, 0)).startIndex();
    if (index < 0 || index >= all.size()) {
        clear();
        return;
    }
    const auto inWord = [&all](int at) {
        const QChar character = all.at(at);
        return character.isLetterOrNumber() || character == u'_' || character == u'\'' || character == u'’';
    };
    if (all.at(index).isSpace()) {
        // Between two words there is no word.
        clear();
        return;
    }
    int start = index;
    int end = index + 1;
    if (inWord(index)) {
        while (start > 0 && inWord(start - 1)) {
            --start;
        }
        while (end < all.size() && inWord(end)) {
            ++end;
        }
    }
    take(page, m_document.getSelectionAtIndex(page, start, end - start));
}

void PdfText::selectPage(int page)
{
    if (page < 0 || page >= m_document.pageCount()) {
        clear();
        return;
    }
    take(page, m_document.getAllText(page));
}

void PdfText::clear()
{
    if (m_page < 0 && m_text.isEmpty() && m_geometry.isEmpty()) {
        return;
    }
    m_page = -1;
    m_text.clear();
    m_geometry.clear();
    Q_EMIT selectionChanged();
}

#include "moc_pdftext.cpp"
