// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "codelayout.h"

#include <QAbstractTextDocumentLayout>
#include <QClipboard>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QScopedValueRollback>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace
{
// Continuation rows are indented with the line up to this share of the width; deeper than that
// and too little room would be left for the text.
constexpr int AlignIndentPercent = 80;
// A block format property without meaning. Changing a block's visibility alone does not make
// the layout measure again; changing its format does.
constexpr int FoldedProperty = QTextFormat::UserProperty + 1;
// The fewest lines kept in the layout while the width changes; see beginWindow().
constexpr int MinWindowLines = 200;
// How much of a folded region its stand-in shows.
constexpr int FoldSummaryLength = 72;
}

CodeLayout::CodeLayout(QObject *parent)
    : QObject(parent)
{
}

QQuickTextDocument *CodeLayout::document() const
{
    return m_document;
}

void CodeLayout::setDocument(QQuickTextDocument *document)
{
    if (m_document == document) {
        return;
    }
    m_document = document;
    m_text.clear();
    m_folds.clear();
    m_formatted = false;
    Q_EMIT documentChanged();
}

bool CodeLayout::isDecorated() const
{
    return m_decorated;
}

void CodeLayout::setDecorated(bool decorated)
{
    if (m_decorated == decorated) {
        return;
    }
    m_decorated = decorated;
    if (!decorated) {
        m_folds.clear();
    }
    Q_EMIT decoratedChanged();
    relayout();
}

bool CodeLayout::isFoldable() const
{
    return m_foldable;
}

void CodeLayout::setFoldable(bool foldable)
{
    if (m_foldable == foldable) {
        return;
    }
    m_foldable = foldable;
    m_folds.clear();
    if (m_partial) {
        const QString text = m_text;
        readText(text);
    } else {
        m_text.clear(); // look at the text again
    }
    Q_EMIT foldableChanged();
}

qreal CodeLayout::topOffset() const
{
    return m_topOffset;
}

void CodeLayout::setTopOffset(qreal offset)
{
    if (!qFuzzyCompare(m_topOffset + 1, offset + 1)) {
        m_topOffset = offset;
        Q_EMIT topOffsetChanged();
    }
}

int CodeLayout::tabWidth() const
{
    return m_tabWidth;
}

void CodeLayout::setTabWidth(int width)
{
    width = qMax(1, width);
    if (m_tabWidth != width) {
        m_tabWidth = width;
        readIndents();
        Q_EMIT tabWidthChanged();
    }
}

QList<int> CodeLayout::numberedLines() const
{
    return m_numberedLines;
}

void CodeLayout::setNumberedLines(const QList<int> &lines)
{
    if (m_numberedLines != lines) {
        m_numberedLines = lines;
        Q_EMIT numbersChanged();
    }
}

QList<int> CodeLayout::lineNumbers() const
{
    return m_lineNumbers;
}

void CodeLayout::setLineNumbers(const QList<int> &numbers)
{
    if (m_lineNumbers != numbers) {
        m_lineNumbers = numbers;
        Q_EMIT numbersChanged();
    }
}

int CodeLayout::largestNumber() const
{
    if (!m_numberedLines.isEmpty() && !m_lineNumbers.isEmpty()) {
        return *std::max_element(m_lineNumbers.cbegin(), m_lineNumbers.cend());
    }
    return qMax(1, int(m_lineStarts.size()));
}

int CodeLayout::lineCount() const
{
    return int(m_lineStarts.size());
}

bool CodeLayout::hasFoldableRegions() const
{
    return !m_regions.isEmpty();
}

bool CodeLayout::hasFolds() const
{
    return !m_folds.isEmpty();
}

int CodeLayout::revision() const
{
    return m_revision;
}

QTextDocument *CodeLayout::textDocument() const
{
    return m_document ? m_document->textDocument() : nullptr;
}

void CodeLayout::readText(const QString &text)
{
    m_text = text;
    m_lineStarts.clear();
    m_lineStarts.append(0);
    for (qsizetype i = text.indexOf(u'\n'); i >= 0; i = text.indexOf(u'\n', i + 1)) {
        m_lineStarts.append(int(i) + 1);
    }
    readIndents();
    m_regions.clear();
    if (m_foldable && m_decorated) {
        findRegions();
    }
    // Folds of regions that are no longer there go.
    m_folds.removeIf([this](int line) {
        return !m_regions.contains(line);
    });
}

void CodeLayout::readIndents()
{
    // How far every line is indented, in columns: what its wrapped rows are set in by.
    m_indents.resize(m_lineStarts.size());
    for (qsizetype line = 0; line < m_lineStarts.size(); ++line) {
        int columns = 0;
        for (qsizetype i = m_lineStarts.at(line); i < m_text.size(); ++i) {
            const QChar c = m_text.at(i);
            if (c == u' ') {
                ++columns;
            } else if (c == u'\t') {
                columns = (columns / m_tabWidth + 1) * m_tabWidth;
            } else {
                break;
            }
        }
        m_indents[line] = columns;
    }
}

void CodeLayout::findRegions()
{
    struct Open {
        int position;
        QChar bracket;
    };
    QList<Open> stack;
    const qsizetype n = m_text.size();
    for (qsizetype i = 0; i < n; ++i) {
        const QChar c = m_text[i];
        if (c == u'"') {
            // Brackets inside strings are text. A string ends with its line at the latest.
            for (++i; i < n && m_text[i] != u'"' && m_text[i] != u'\n'; ++i) {
                if (m_text[i] == u'\\') {
                    ++i;
                }
            }
        } else if (c == u'{' || c == u'[') {
            stack.append({int(i), c});
        } else if (c == u'}' || c == u']') {
            const QChar wanted = c == u'}' ? u'{' : u'[';
            if (stack.isEmpty() || stack.constLast().bracket != wanted) {
                continue; // does not match; brackets in prose and comments
            }
            const Open open = stack.takeLast();
            Region region;
            region.open = open.position;
            region.close = int(i);
            region.startLine = lineOfSource(region.open);
            region.endLine = lineOfSource(region.close);
            region.topLevel = stack.isEmpty();
            if (region.endLine <= region.startLine) {
                continue;
            }
            // The outermost region that begins on a line is the one that line folds.
            const auto existing = m_regions.constFind(region.startLine);
            if (existing == m_regions.constEnd() || region.close > existing->close) {
                qsizetype lineEnd = m_text.indexOf(u'\n', region.close);
                if (lineEnd < 0) {
                    lineEnd = n;
                }
                region.closing = m_text.mid(region.close, lineEnd - region.close).trimmed();
                m_regions.insert(region.startLine, region);
            }
        }
    }
}

bool CodeLayout::isFolded(const Region &region) const
{
    return m_folds.contains(region.startLine);
}

QList<bool> CodeLayout::hiddenLines() const
{
    QList<bool> hidden(m_lineStarts.size(), false);
    for (const int line : m_folds) {
        const auto region = m_regions.constFind(line);
        if (region == m_regions.constEnd()) {
            continue;
        }
        for (int l = region->startLine + 1; l <= region->endLine && l < hidden.size(); ++l) {
            hidden[l] = true;
        }
    }
    return hidden;
}

void CodeLayout::applyHiddenLines(QTextDocument *doc, const QList<bool> &hidden)
{
    std::unique_ptr<QTextCursor> cursor;
    int line = m_partial ? m_firstLine : 0;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next(), ++line) {
        const bool visible = line >= hidden.size() || !hidden.at(line);
        if (block.isVisible() == visible) {
            continue;
        }
        block.setVisible(visible);
        if (!cursor) {
            cursor = std::make_unique<QTextCursor>(doc);
            cursor->beginEditBlock();
        }
        cursor->setPosition(block.position());
        QTextBlockFormat format = cursor->blockFormat();
        format.setProperty(FoldedProperty, !visible);
        cursor->setBlockFormat(format);
    }
    if (cursor) {
        cursor->endEditBlock();
    }
}

void CodeLayout::applyHangingIndent(QTextDocument *doc, bool enabled)
{
    // A wrapped line continues below its own first character instead of at the left edge:
    // the block gets a left margin as wide as its indentation and its first row is pulled
    // back by the same amount.
    const qreal space = QFontMetricsF(doc->defaultFont()).horizontalAdvance(u' ');
    const qreal limit = doc->textWidth() > 0 ? doc->textWidth() * AlignIndentPercent / 100 : std::numeric_limits<qreal>::max();

    QList<std::pair<int, qreal>> changes;
    int line = m_partial ? m_firstLine : 0;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next(), ++line) {
        qreal indent = 0;
        if (enabled && line < m_indents.size()) {
            indent = m_indents.at(line) * space;
            if (indent > limit) {
                indent = 0;
            }
        }
        const QTextBlockFormat format = block.blockFormat();
        if (qAbs(format.leftMargin() - indent) > 0.01 || qAbs(format.textIndent() + indent) > 0.01) {
            changes.append({block.position(), indent});
        }
    }
    if (changes.isEmpty()) {
        return;
    }
    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    for (const auto &[position, indent] : std::as_const(changes)) {
        cursor.setPosition(position);
        QTextBlockFormat format = cursor.blockFormat();
        format.setLeftMargin(indent);
        format.setTextIndent(-indent);
        cursor.setBlockFormat(format);
    }
    cursor.endEditBlock();
}

void CodeLayout::relayout()
{
    QTextDocument *doc = textDocument();
    if (!doc || m_working) {
        return; // formatting the document reports a change, which asks for a relayout
    }
    const QScopedValueRollback working(m_working, true);

    if (m_window) {
        // Only where the lines in the layout are now is of interest, and has to be cheap:
        // this is asked for with every step the width changes by.
        readTops(doc, m_windowFirst, m_windowLast);
        ++m_revision;
        Q_EMIT layoutChanged();
        return;
    }

    if (m_partial) {
        if (!m_held) {
            return;
        }
        if (doc->isUndoRedoEnabled()) {
            doc->setUndoRedoEnabled(false);
        }
        applyHiddenLines(doc, hiddenLines());
        applyHangingIndent(doc, true);
        m_formatted = true;
        readTops(doc, m_firstLine, m_lastLine);
        ++m_revision;
        Q_EMIT layoutChanged();
        return;
    }

    const QString text = doc->toPlainText();
    if (text != m_text || m_lineStarts.isEmpty()) {
        readText(text);
    }

    const QList<bool> hidden = hiddenLines();
    if (m_decorated) {
        if (doc->isUndoRedoEnabled()) {
            doc->setUndoRedoEnabled(false);
        }
        applyHiddenLines(doc, hidden);
        applyHangingIndent(doc, true);
        m_formatted = true;
    } else if (m_formatted) {
        // Hand the document back as plain as it came, then let it keep a history again.
        applyHiddenLines(doc, {});
        applyHangingIndent(doc, false);
        m_formatted = false;
        doc->setUndoRedoEnabled(true);
    }

    readTops(doc, 0, int(m_lineStarts.size()) - 1);

    ++m_revision;
    Q_EMIT layoutChanged();
}

void CodeLayout::readTops(QTextDocument *doc, int firstLine, int lastLine)
{
    m_tops.resize(m_lineStarts.size());
    m_tops.fill(-1);
    m_top = m_topOffset;
    m_bottom = m_topOffset;
    const QAbstractTextDocumentLayout *layout = doc->documentLayout();
    // Asking for the size makes the layout catch up with the formatting just applied; the
    // positions of single blocks are otherwise those from before.
    layout->documentSize();
    const int held = m_partial ? m_firstLine : 0; // the line the document begins with
    int line = qMax(held, firstLine);
    bool first = true;
    for (QTextBlock block = doc->findBlockByNumber(line - held); block.isValid() && line < m_tops.size() && line <= lastLine; block = block.next(), ++line) {
        if (!block.isVisible()) {
            continue;
        }
        const QRectF rect = layout->blockBoundingRect(block);
        m_tops[line] = rect.top() + m_topOffset;
        m_bottom = rect.bottom() + m_topOffset;
        if (first) {
            m_top = m_tops.at(line);
            first = false;
        }
    }
}

int CodeLayout::lineAtY(qreal y) const
{
    int found = -1;
    // Lines that are folded away have no place; the search goes by those that have one.
    qsizetype low = m_partial ? m_firstLine : 0;
    qsizetype high = m_partial ? qMin<qsizetype>(m_lastLine, m_tops.size() - 1) : m_tops.size() - 1;
    while (low <= high) {
        const qsizetype middle = (low + high) / 2;
        qsizetype probe = middle;
        while (probe >= low && m_tops.at(probe) < 0) {
            --probe;
        }
        if (probe < low) {
            low = middle + 1; // nothing shown in the lower half of this stretch
            continue;
        }
        if (m_tops.at(probe) <= y) {
            found = int(probe);
            low = middle + 1;
        } else {
            high = probe - 1;
        }
    }
    if (found >= 0) {
        return found;
    }
    // Above everything: the first line that is shown at all.
    for (qsizetype line = m_partial ? m_firstLine : 0; line < m_tops.size(); ++line) {
        if (m_tops.at(line) >= 0) {
            return int(line);
        }
    }
    return m_partial ? m_firstLine : 0;
}

int CodeLayout::shownLineNear(int line) const
{
    if (m_tops.isEmpty()) {
        return 0;
    }
    line = qBound(0, line, int(m_tops.size()) - 1);
    for (int up = line; up >= 0 && (!m_partial || up >= m_firstLine); --up) {
        if (m_tops.at(up) >= 0) {
            return up;
        }
    }
    for (int down = line; down < m_tops.size(); ++down) {
        if (m_tops.at(down) >= 0) {
            return down;
        }
    }
    return line;
}

bool CodeLayout::isPartial() const
{
    return m_partial;
}

int CodeLayout::firstLine() const
{
    return m_partial ? m_firstLine : 0;
}

int CodeLayout::lastLine() const
{
    return m_partial ? m_lastLine : int(m_lineStarts.size()) - 1;
}

qreal CodeLayout::top() const
{
    return m_top;
}

qreal CodeLayout::bottom() const
{
    return m_bottom;
}

int CodeLayout::length() const
{
    return int(m_text.size());
}

void CodeLayout::setSource(const QString &text)
{
    m_partial = true;
    m_held = false;
    m_firstLine = 0;
    m_lastLine = -1;
    m_window = false;
    m_folds.clear();
    readText(text);
    m_tops.clear();
}

void CodeLayout::clearSource()
{
    if (!m_partial) {
        return;
    }
    m_partial = false;
    m_held = false;
    m_firstLine = 0;
    m_lastLine = -1;
    m_folds.clear();
    m_text.clear(); // read from the document at the next relayout()
    m_lineStarts.clear();
    m_tops.clear();
}

QVariantMap CodeLayout::linesAround(int line, int charactersBefore, int charactersAfter, int linesBefore, int linesAfter) const
{
    const int lines = int(m_lineStarts.size());
    if (lines == 0) {
        return {{u"first"_s, 0}, {u"last"_s, 0}};
    }
    line = qBound(0, line, lines - 1);
    const QList<bool> hidden = m_decorated ? hiddenLines() : QList<bool>();
    const auto shown = [&hidden](int l) {
        return l >= hidden.size() || !hidden.at(l);
    };

    int first = line;
    qint64 characters = 0;
    int count = 0;
    while (first > 0 && (characters < charactersBefore || count < linesBefore)) {
        --first;
        if (shown(first)) {
            characters += lineLength(first);
            ++count;
        }
    }
    // Not from the middle of something folded: its first line is what is shown of it.
    while (first > 0 && !shown(first)) {
        --first;
    }

    int last = line;
    characters = shown(line) ? lineLength(line) : 0;
    count = shown(line) ? 1 : 0;
    while (last < lines - 1 && (characters < charactersAfter || count < linesAfter)) {
        ++last;
        if (shown(last)) {
            characters += lineLength(last);
            ++count;
        }
    }
    return {{u"first"_s, first}, {u"last"_s, last}};
}

void CodeLayout::showLines(int first, int last)
{
    QTextDocument *doc = textDocument();
    const int lines = int(m_lineStarts.size());
    if (!doc || !m_partial || lines == 0) {
        return;
    }
    first = qBound(0, first, lines - 1);
    last = qBound(first, last, lines - 1);
    if (m_held && first == m_firstLine && last == m_lastLine) {
        return;
    }
    if (doc->isUndoRedoEnabled()) {
        doc->setUndoRedoEnabled(false);
    }
    const QStringView text(m_text);
    const auto slice = [this, text](int from, int to) {
        return text.mid(sourcePosition(from), lineEnd(to) - sourcePosition(from)).toString();
    };
    bool startedOver = false;
    {
        // What the document does while it is being changed asks for relayouts; one at the
        // end will do.
        const QScopedValueRollback working(m_working, true);
        QTextCursor cursor(doc);
        cursor.beginEditBlock();
        if (!m_held || first > m_lastLine || last < m_firstLine) {
            // Nothing in common with what is held: start over.
            cursor.select(QTextCursor::Document);
            cursor.insertText(slice(first, last));
            startedOver = true;
        } else {
            // Lines stay: add and take away at the ends only, the end first so that the
            // places at the beginning are still what they were.
            if (last > m_lastLine) {
                cursor.movePosition(QTextCursor::End);
                cursor.insertText(u'\n' + slice(m_lastLine + 1, last));
            } else if (last < m_lastLine) {
                cursor.setPosition(lineEnd(last) - sourcePosition(m_firstLine));
                cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
            }
            if (first < m_firstLine) {
                cursor.setPosition(0);
                cursor.insertText(slice(first, m_firstLine - 1) + u'\n');
            } else if (first > m_firstLine) {
                cursor.setPosition(0);
                cursor.setPosition(sourcePosition(first) - sourcePosition(m_firstLine), QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
            }
        }
        cursor.endEditBlock();
        m_firstLine = first;
        m_lastLine = last;
        m_held = true;
    }
    relayout();
    if (startedOver) {
        redraw();
    }
}

void CodeLayout::redraw()
{
    // The TextEdit's own way of starting over. It is not part of its interface for QML, but
    // it is a slot, and there is no other way to ask for it; without it the call does nothing.
    if (m_document && m_document->parent()) {
        QMetaObject::invokeMethod(m_document->parent(), "q_invalidate");
    }
}

void CodeLayout::copySource() const
{
    QGuiApplication::clipboard()->setText(m_text);
}

qreal CodeLayout::beginWindow(int firstLine, int characters, int shownLines)
{
    QTextDocument *doc = textDocument();
    if (!doc || m_window || m_tops.isEmpty()) {
        return 0;
    }
    const int lines = int(m_tops.size());
    firstLine = qBound(0, firstLine, lines - 1);
    while (firstLine > 0 && m_tops.at(firstLine) < 0) {
        --firstLine;
    }
    // Enough to fill the view however much wider it gets, counted in characters since that
    // is what wrapping costs by.
    int lastLine = lineAt(lineStart(firstLine) + qMax(0, characters));
    lastLine = qMin(lines - 1, qMax(lastLine, firstLine + MinWindowLines));
    const QList<bool> folded = m_decorated ? hiddenLines() : QList<bool>();
    // Folded lines take no room: with many of them it takes more lines to fill the view.
    int shown = 0;
    for (int line = firstLine; line <= lastLine; ++line) {
        shown += line >= folded.size() || !folded.at(line) ? 1 : 0;
    }
    while (shown < shownLines && lastLine < lines - 1) {
        ++lastLine;
        shown += lastLine >= folded.size() || !folded.at(lastLine) ? 1 : 0;
    }

    // With everything above it out of the layout, the first line of the window comes to
    // stand where the first line of the text stood.
    qreal firstTop = -1;
    for (const qreal top : std::as_const(m_tops)) {
        if (top >= 0) {
            firstTop = top;
            break;
        }
    }
    const qreal moved = firstTop >= 0 && m_tops.at(firstLine) >= 0 ? m_tops.at(firstLine) - firstTop : 0;

    // Only told to the blocks, not to the document as a change: that would have every line
    // coloured anew. The layout learns of it with the change of width this is done for.
    int line = 0;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next(), ++line) {
        const bool visible = line >= firstLine && line <= lastLine && (line >= folded.size() || !folded.at(line));
        if (block.isVisible() != visible) {
            block.setVisible(visible);
        }
    }
    m_window = true;
    m_windowFirst = firstLine;
    m_windowLast = lastLine;
    return moved;
}

void CodeLayout::endWindow()
{
    QTextDocument *doc = textDocument();
    if (!m_window) {
        return;
    }
    m_window = false;
    if (!doc) {
        return;
    }
    const QList<bool> folded = m_decorated ? hiddenLines() : QList<bool>();
    int line = 0;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next(), ++line) {
        const bool visible = line >= folded.size() || !folded.at(line);
        if (block.isVisible() != visible) {
            block.setVisible(visible);
        }
    }
}

QString CodeLayout::labelFor(int line) const
{
    if (m_numberedLines.isEmpty()) {
        return QString::number(line + 1);
    }
    const auto it = std::lower_bound(m_numberedLines.cbegin(), m_numberedLines.cend(), line);
    if (it == m_numberedLines.cend() || *it != line) {
        return {};
    }
    const qsizetype index = it - m_numberedLines.cbegin();
    return index < m_lineNumbers.size() ? QString::number(m_lineNumbers.at(index)) : QString();
}

QVariantList CodeLayout::visibleLines(qreal top, qreal bottom) const
{
    QVariantList result;
    if (m_tops.isEmpty()) {
        return result;
    }
    // The last line that starts at or above `top`. Folded lines have no position; the search
    // looks at the nearest line above them instead.
    const qsizetype heldFirst = m_partial ? m_firstLine : 0;
    const qsizetype heldLast = m_partial ? qMin<qsizetype>(m_lastLine, m_tops.size() - 1) : m_tops.size() - 1;
    qsizetype low = heldFirst;
    qsizetype high = heldLast;
    while (low < high) {
        const qsizetype middle = (low + high + 1) / 2;
        qsizetype probe = middle;
        while (probe > low && m_tops.at(probe) < 0) {
            --probe;
        }
        if (m_tops.at(probe) <= top) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    while (low > heldFirst && m_tops.at(low) < 0) {
        --low;
    }
    for (qsizetype line = low; line <= heldLast; ++line) {
        const qreal y = m_tops.at(line);
        if (y < 0) {
            continue;
        }
        if (y > bottom) {
            break;
        }
        const auto region = m_regions.constFind(int(line));
        const bool foldable = region != m_regions.constEnd();
        result.append(QVariantMap{
            {u"line"_s, int(line)},
            {u"label"_s, labelFor(int(line))},
            {u"y"_s, y},
            {u"foldable"_s, foldable},
            {u"folded"_s, foldable && isFolded(*region)},
        });
    }
    return result;
}

QVariantList CodeLayout::foldBadges(qreal top, qreal bottom) const
{
    QVariantList result;
    for (const int line : m_folds) {
        const auto region = m_regions.constFind(line);
        if (region == m_regions.constEnd() || line >= m_tops.size()) {
            continue;
        }
        const qreal y = m_tops.at(line);
        if (y < 0 || y > bottom || y + lineHeight(line) < top) {
            continue;
        }
        const int end = line + 1 < m_lineStarts.size() ? m_lineStarts.at(line + 1) - 1 : int(m_text.size());
        // The beginning of what is hidden, on one line, so that folded records can still be
        // told apart. Plain dots: monospace fonts often lack "…", and the stand-in is wider.
        const int hiddenFrom = qMin(end + 1, region->close);
        QString summary = QStringView(m_text).mid(hiddenFrom, qMin(region->close - hiddenFrom, 4 * FoldSummaryLength)).toString().simplified();
        if (summary.size() > FoldSummaryLength) {
            summary.truncate(FoldSummaryLength);
        }
        result.append(QVariantMap{
            {u"line"_s, line},
            {u"position"_s, end - base()},
            {u"text"_s, QString(summary + " ... "_L1 + region->closing)},
        });
    }
    return result;
}

int CodeLayout::lineOfSource(int position) const
{
    const auto it = std::upper_bound(m_lineStarts.cbegin(), m_lineStarts.cend(), position);
    return qMax(0, int(it - m_lineStarts.cbegin()) - 1);
}

int CodeLayout::sourcePosition(int line) const
{
    return line >= 0 && line < m_lineStarts.size() ? m_lineStarts.at(line) : 0;
}

int CodeLayout::lineEnd(int line) const
{
    return line + 1 < m_lineStarts.size() ? m_lineStarts.at(line + 1) - 1 : int(m_text.size());
}

int CodeLayout::lineLength(int line) const
{
    return line >= 0 && line < m_lineStarts.size() ? lineEnd(line) - m_lineStarts.at(line) + 1 : 0;
}

int CodeLayout::base() const
{
    return m_partial ? sourcePosition(m_firstLine) : 0;
}

int CodeLayout::documentPosition(int sourcePosition) const
{
    if (!m_partial) {
        return sourcePosition;
    }
    if (!m_held || sourcePosition < base() || sourcePosition > lineEnd(m_lastLine)) {
        return -1;
    }
    return sourcePosition - base();
}

int CodeLayout::lineAt(int position) const
{
    return lineOfSource(position + base());
}

int CodeLayout::lineStart(int line) const
{
    return sourcePosition(line) - base();
}

qreal CodeLayout::lineTop(int line) const
{
    return line >= 0 && line < m_tops.size() ? m_tops.at(line) : -1;
}

qreal CodeLayout::lineHeight(int line) const
{
    const qreal top = lineTop(line);
    if (top < 0) {
        return 0;
    }
    const qsizetype heldLast = m_partial ? qMin<qsizetype>(m_lastLine, m_tops.size() - 1) : m_tops.size() - 1;
    for (qsizetype next = line + 1; next <= heldLast; ++next) {
        if (m_tops.at(next) >= 0) {
            return m_tops.at(next) - top;
        }
    }
    return m_bottom - top;
}

void CodeLayout::toggleFold(int line)
{
    if (!m_regions.contains(line)) {
        return;
    }
    if (!m_folds.removeOne(line)) {
        m_folds.append(line);
    }
    relayout();
}

void CodeLayout::foldTopLevel(bool fold)
{
    for (const Region &region : std::as_const(m_regions)) {
        if (!region.topLevel) {
            continue;
        }
        if (fold && !m_folds.contains(region.startLine)) {
            m_folds.append(region.startLine);
        } else if (!fold) {
            m_folds.removeOne(region.startLine);
        }
    }
    relayout();
}

void CodeLayout::unfoldAll()
{
    if (!m_folds.isEmpty()) {
        m_folds.clear();
        relayout();
    }
}

void CodeLayout::reveal(int position)
{
    const int line = lineAt(position);
    const qsizetype removed = m_folds.removeIf([this, line](int fold) {
        const auto region = m_regions.constFind(fold);
        return region != m_regions.constEnd() && line > region->startLine && line <= region->endLine;
    });
    if (removed > 0) {
        relayout();
    }
}

#include "moc_codelayout.cpp"
