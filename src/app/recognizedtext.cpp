// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "recognizedtext.h"

#include "app.h"

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace Indeks;

namespace
{
/** More places than this are not listed: nobody goes through them. */
constexpr int MaxHits = 5000;
constexpr int ContextBefore = 40;
constexpr int ContextAfter = 80;
}

RecognizedText::RecognizedText(QObject *parent)
    : QObject(parent)
{
    // The words of a scan from before Indeks kept them arrive a while after it is shown.
    connect(App::instance()->library(), &Library::recognizedLayoutChanged, this, [this](qint64 id) {
        if (id == m_documentId) {
            load();
        }
    });
}

void RecognizedText::setDocumentId(qint64 id)
{
    if (id == m_documentId) {
        return;
    }
    m_documentId = id;
    Q_EMIT documentIdChanged();
    load();
}

void RecognizedText::setOwnTextPages(const QList<int> &pages)
{
    if (pages == m_ownTextPages) {
        return;
    }
    m_ownTextPages = pages;
    m_skipped = QSet<int>(pages.cbegin(), pages.cend());
    m_lines.clear();
    clear();
    ++m_revision;
    Q_EMIT handledChanged();
    find();
}

void RecognizedText::load()
{
    m_pages = m_documentId > 0 ? App::instance()->library()->recognizedLayout(m_documentId) : QList<Ocr::Page>();
    m_lines.clear();
    clear();
    ++m_revision;
    Q_EMIT layoutChanged();
    Q_EMIT handledChanged();
    find();
}

bool RecognizedText::isAvailable() const
{
    for (int page = 0; page < m_pages.size(); ++page) {
        if (hasWords(page)) {
            return true;
        }
    }
    return false;
}

bool RecognizedText::hasWords(int page) const
{
    return page >= 0 && page < m_pages.size() && !m_pages.at(page).isEmpty() && !m_skipped.contains(page);
}

const QList<RecognizedText::Line> &RecognizedText::lines(int page)
{
    auto known = m_lines.find(page);
    if (known == m_lines.end()) {
        QList<Line> found;
        if (hasWords(page)) {
            const Ocr::Page &words = m_pages.at(page);
            for (int i = 0; i < words.size(); ++i) {
                if (found.isEmpty() || words.at(i).line != words.at(found.constLast().last).line) {
                    found.append({words.at(i).box, i, i});
                } else {
                    found.last().rect = found.constLast().rect.united(words.at(i).box);
                    found.last().last = i;
                }
            }
        }
        known = m_lines.insert(page, found);
    }
    return *known;
}

int RecognizedText::nearest(int page, const QPointF &point)
{
    // As PdfText does: the line at the height of the place, or the one nearest to that
    // height, and of several at one height the one nearest sideways; on it, the word the
    // place is in, or the one nearest to it.
    const QList<Line> &all = lines(page);
    const auto beside = [&point](const QRectF &rect) {
        return point.x() < rect.left() ? rect.left() - point.x() : point.x() > rect.right() ? point.x() - rect.right() : 0;
    };
    const Line *best = nullptr;
    qreal bestAbove = 0;
    qreal bestBeside = 0;
    for (const Line &line : all) {
        const QRectF &rect = line.rect;
        const qreal above = point.y() < rect.top() ? rect.top() - point.y() : point.y() > rect.bottom() ? point.y() - rect.bottom() : 0;
        const qreal sideways = beside(rect);
        if (!best || above < bestAbove || (above == bestAbove && sideways < bestBeside)) {
            best = &line;
            bestAbove = above;
            bestBeside = sideways;
        }
    }
    if (!best) {
        return -1;
    }
    const Ocr::Page &words = m_pages.at(page);
    int word = best->first;
    for (int i = best->first; i <= best->last; ++i) {
        if (beside(words.at(i).box) < beside(words.at(word).box)) {
            word = i;
        }
    }
    return word;
}

QList<QPolygonF> RecognizedText::shapes(const QList<QRectF> &rects)
{
    QList<QPolygonF> result;
    result.reserve(rects.size());
    for (const QRectF &rect : rects) {
        result.append(QPolygonF(rect));
    }
    return result;
}

void RecognizedText::take(int page, int first, int last)
{
    const Ocr::Page &words = m_pages.at(page);
    const Ocr::Page chosen = words.mid(first, last - first + 1);
    // One shape for the words of each line, so that the selection reads as lines and not
    // as a row of boxes.
    QList<QRectF> rects;
    int line = -1;
    for (const Ocr::Word &word : chosen) {
        if (word.line != line) {
            rects.append(word.box);
            line = word.line;
        } else {
            rects.last() = rects.constLast().united(word.box);
        }
    }
    m_page = page;
    m_text = Ocr::textOf(chosen);
    m_geometry = shapes(rects);
    Q_EMIT selectionChanged();
}

void RecognizedText::selectBetween(int page, const QPointF &from, const QPointF &to)
{
    if (!hasWords(page)) {
        clear();
        return;
    }
    int first = nearest(page, from);
    int last = nearest(page, to);
    if (first < 0 || last < 0) {
        clear();
        return;
    }
    // From the earlier to the later word, whichever way the mouse went.
    if (last < first) {
        std::swap(first, last);
    }
    take(page, first, last);
}

void RecognizedText::selectWordAt(int page, const QPointF &point)
{
    const int word = hasWords(page) ? nearest(page, point) : -1;
    // Only a word that is pointed at, give or take a little: a double click into the empty
    // part of a page is not meant for the nearest word.
    if (word < 0 || !m_pages.at(page).at(word).box.adjusted(-0.01, -0.01, 0.01, 0.01).contains(point)) {
        clear();
        return;
    }
    take(page, word, word);
}

void RecognizedText::selectPage(int page)
{
    if (!hasWords(page)) {
        clear();
        return;
    }
    take(page, 0, int(m_pages.at(page).size()) - 1);
}

void RecognizedText::clear()
{
    if (m_page < 0 && m_text.isEmpty() && m_geometry.isEmpty()) {
        return;
    }
    m_page = -1;
    m_text.clear();
    m_geometry.clear();
    Q_EMIT selectionChanged();
}

void RecognizedText::setFindText(const QString &text)
{
    const QString wanted = text.simplified();
    if (wanted == m_findText) {
        return;
    }
    m_findText = wanted;
    find();
}

void RecognizedText::find()
{
    const bool hadCurrent = m_current >= 0;
    m_hits.clear();
    m_current = -1;
    if (!m_findText.isEmpty()) {
        for (int page = 0; page < m_pages.size() && m_hits.size() < MaxHits; ++page) {
            if (!hasWords(page)) {
                continue;
            }
            // The words of the page as one line of text, with a space between them, and where
            // in it each word begins: what is looked for may reach across words.
            const Ocr::Page &words = m_pages.at(page);
            QString joined;
            QList<int> starts;
            starts.reserve(words.size());
            for (const Ocr::Word &word : words) {
                if (!joined.isEmpty()) {
                    joined += u' ';
                }
                starts.append(int(joined.size()));
                joined += word.text;
            }
            for (qsizetype at = joined.indexOf(m_findText, 0, Qt::CaseInsensitive); at >= 0 && m_hits.size() < MaxHits;
                 at = joined.indexOf(m_findText, at + m_findText.size(), Qt::CaseInsensitive)) {
                const int end = int(at + m_findText.size()); // one past the last letter
                const int first = int(std::upper_bound(starts.cbegin(), starts.cend(), int(at)) - starts.cbegin()) - 1;
                const int last = int(std::upper_bound(starts.cbegin(), starts.cend(), end - 1) - starts.cbegin()) - 1;
                Hit hit;
                hit.page = page;
                int line = -1;
                for (int i = first; i <= last; ++i) {
                    // Of a word that is found in part, as much of its width as the letters
                    // found are of its letters: they are taken to be of one width.
                    const Ocr::Word &word = words.at(i);
                    const qreal length = qMax<qsizetype>(1, word.text.size());
                    const qreal from = qBound<qreal>(0, (at - starts.at(i)) / length, 1);
                    const qreal to = qBound<qreal>(0, (end - starts.at(i)) / length, 1);
                    QRectF rect = word.box;
                    rect.setLeft(word.box.left() + word.box.width() * from);
                    rect.setRight(word.box.left() + word.box.width() * to);
                    if (rect.width() <= 0) {
                        continue;
                    }
                    if (word.line != line) {
                        hit.rects.append(rect);
                        line = word.line;
                    } else {
                        hit.rects.last() = hit.rects.constLast().united(rect);
                    }
                }
                if (hit.rects.isEmpty()) {
                    continue;
                }
                hit.match = joined.mid(at, m_findText.size());
                hit.before = joined.mid(qMax<qsizetype>(0, at - ContextBefore), qMin<qsizetype>(at, ContextBefore));
                if (at > ContextBefore) {
                    // Not from the middle of a word.
                    const qsizetype space = hit.before.indexOf(u' ');
                    hit.before = space >= 0 ? hit.before.mid(space + 1) : hit.before;
                }
                hit.after = joined.mid(end, ContextAfter);
                m_hits.append(hit);
            }
        }
    }
    Q_EMIT findChanged();
    if (hadCurrent) {
        Q_EMIT currentChanged();
    }
}

QVariantMap RecognizedText::hit(int index) const
{
    if (index < 0 || index >= m_hits.size()) {
        return {};
    }
    const Hit &hit = m_hits.at(index);
    return {
        {u"page"_s, hit.page},
        {u"before"_s, hit.before},
        {u"match"_s, hit.match},
        {u"after"_s, hit.after},
        {u"top"_s, hit.rects.constFirst().top()},
    };
}

QList<QPolygonF> RecognizedText::marks(int page) const
{
    QList<QPolygonF> result;
    for (const Hit &hit : m_hits) {
        if (hit.page == page) {
            result += shapes(hit.rects);
        }
    }
    return result;
}

QList<QPolygonF> RecognizedText::currentMarks() const
{
    return m_current >= 0 && m_current < m_hits.size() ? shapes(m_hits.at(m_current).rects) : QList<QPolygonF>();
}

void RecognizedText::setCurrent(int index)
{
    const int wanted = index >= 0 && index < m_hits.size() ? index : -1;
    if (wanted != m_current) {
        m_current = wanted;
        Q_EMIT currentChanged();
    }
}

#include "moc_recognizedtext.cpp"
