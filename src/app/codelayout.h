// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QTextDocument;

/**
 * What a TextEdit needs to work as a code view, and does not do by itself: where each line of
 * the text sits (for line numbers beside it), continuation rows of wrapped lines that line up
 * with the line's indentation, and folding of bracketed regions.
 *
 * It works on the TextEdit's own document. The view asks for relayout() whenever text or width
 * changed and then draws from visibleLines() and foldBadges().
 */
class CodeLayout : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    /** The `textDocument` of the TextEdit. */
    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    /**
     * Hanging indents and folding are formatting applied to the document, which would end up
     * among the user's undo steps. They are only used while this is set, that is while the text
     * is being read; setting it switches the document's undo history off, clearing it gives it
     * back.
     */
    Q_PROPERTY(bool decorated READ isDecorated WRITE setDecorated NOTIFY decoratedChanged)
    /** Whether to look for regions to fold: brackets that span lines, outside "strings". */
    Q_PROPERTY(bool foldable READ isFoldable WRITE setFoldable NOTIFY foldableChanged)
    /** Distance from the top of the TextEdit to the top of its text. */
    Q_PROPERTY(qreal topOffset READ topOffset WRITE setTopOffset NOTIFY topOffsetChanged)
    /** Width of a tab in spaces, for lining up wrapped rows. */
    Q_PROPERTY(int tabWidth READ tabWidth WRITE setTabWidth NOTIFY tabWidthChanged)
    /**
     * If not empty: the only lines (from 0, ascending) that carry a number, and the numbers.
     * Used when the text shown is a layout of the file's, where a record that is one line of
     * the file takes many.
     */
    Q_PROPERTY(QList<int> numberedLines READ numberedLines WRITE setNumberedLines NOTIFY numbersChanged)
    Q_PROPERTY(QList<int> lineNumbers READ lineNumbers WRITE setLineNumbers NOTIFY numbersChanged)
    /** The largest line number there is to show; the gutter is as wide as this needs. */
    Q_PROPERTY(int largestNumber READ largestNumber NOTIFY layoutChanged)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY layoutChanged)
    Q_PROPERTY(bool hasFoldableRegions READ hasFoldableRegions NOTIFY layoutChanged)
    Q_PROPERTY(bool hasFolds READ hasFolds NOTIFY layoutChanged)
    /** Changes with every relayout(); bind to it to follow the layout. */
    Q_PROPERTY(int revision READ revision NOTIFY layoutChanged)
    /**
     * Whether the document holds only some lines of a longer text; see setSource(). The lines
     * it holds are firstLine to lastLine. Lines are counted in the whole text everywhere.
     */
    Q_PROPERTY(bool partial READ isPartial NOTIFY layoutChanged)
    Q_PROPERTY(int firstLine READ firstLine NOTIFY layoutChanged)
    Q_PROPERTY(int lastLine READ lastLine NOTIFY layoutChanged)
    /** Where the lines that are laid out begin and end, in TextEdit coordinates. */
    Q_PROPERTY(qreal top READ top NOTIFY layoutChanged)
    Q_PROPERTY(qreal bottom READ bottom NOTIFY layoutChanged)
    /** Number of characters of the whole text. */
    Q_PROPERTY(int length READ length NOTIFY layoutChanged)
public:
    explicit CodeLayout(QObject *parent = nullptr);

    QQuickTextDocument *document() const;
    void setDocument(QQuickTextDocument *document);
    bool isDecorated() const;
    void setDecorated(bool decorated);
    bool isFoldable() const;
    void setFoldable(bool foldable);
    qreal topOffset() const;
    void setTopOffset(qreal offset);
    int tabWidth() const;
    void setTabWidth(int width);
    QList<int> numberedLines() const;
    void setNumberedLines(const QList<int> &lines);
    QList<int> lineNumbers() const;
    void setLineNumbers(const QList<int> &numbers);
    int largestNumber() const;
    int lineCount() const;
    bool hasFoldableRegions() const;
    bool hasFolds() const;
    int revision() const;
    bool isPartial() const;
    int firstLine() const;
    int lastLine() const;
    qreal top() const;
    qreal bottom() const;
    int length() const;

    /**
     * For texts too long to put into a TextEdit whole. Laying a text out, and wrapping it
     * anew when the width changes, costs time for every line the TextEdit holds, whether it
     * is in view or not; megabytes make scrolling and resizing crawl. So the whole text is
     * given here and the document gets the lines around the view only: showLines() says
     * which, linesAround() helps to choose. Line numbers, folding and the positions of
     * lines go by the whole text; positions in the document (as lineAt() takes and
     * lineStart() gives them) by what it holds.
     *
     * Only for reading: the document is not the text. clearSource() goes back to a
     * document that holds everything.
     */
    Q_INVOKABLE void setSource(const QString &text);
    Q_INVOKABLE void clearSource();
    /**
     * The lines to hold for a view whose first line is @p line: back and forth until both
     * the characters and the lines asked for are there, counting only what is not folded
     * away. Returns `first` and `last`.
     */
    Q_INVOKABLE QVariantMap linesAround(int line, int charactersBefore, int charactersAfter, int linesBefore, int linesAfter) const;
    /** Makes the document hold the lines @p first to @p last and lays them out. */
    Q_INVOKABLE void showLines(int first, int last);
    /** Where a line begins in the whole text, how long it is, and the line a place is in. */
    Q_INVOKABLE int sourcePosition(int line) const;
    Q_INVOKABLE int lineLength(int line) const;
    Q_INVOKABLE int lineOfSource(int position) const;
    /** Position in the document of a place in the whole text; negative if it is not held. */
    Q_INVOKABLE int documentPosition(int sourcePosition) const;
    /** The nearest line to @p line that is shown: itself, or one above, or one below. */
    Q_INVOKABLE int shownLineNear(int line) const;
    /** Puts the whole text on the clipboard. */
    Q_INVOKABLE void copySource() const;
    /**
     * To be called when the TextEdit got another text altogether. It draws only the lines in
     * view and, after a change, only from the first line it had drawn before: with a new
     * text, shown from another place, that leaves the lines before that place undrawn. This
     * makes it draw everything anew.
     */
    Q_INVOKABLE void redraw();

    /** Brings formatting and line positions in line with the document as it is now. */
    Q_INVOKABLE void relayout();

    /**
     * The lines between @p top and @p bottom (in TextEdit coordinates) that are not folded
     * away: maps with `line` (from 0), `label` (the number to show, may be empty), `y`,
     * `foldable` and `folded`.
     */
    Q_INVOKABLE QVariantList visibleLines(qreal top, qreal bottom) const;
    /** For every folded region in sight: `line`, `position` (end of that line) and `text`. */
    Q_INVOKABLE QVariantList foldBadges(qreal top, qreal bottom) const;

    Q_INVOKABLE int lineAt(int position) const;
    Q_INVOKABLE int lineStart(int line) const;
    /** Top of a line; negative if it is folded away. */
    Q_INVOKABLE qreal lineTop(int line) const;
    /** Height of a line with all its wrapped rows. */
    Q_INVOKABLE qreal lineHeight(int line) const;

    Q_INVOKABLE void toggleFold(int line);
    /** Folds or unfolds every region that is not inside another one. */
    Q_INVOKABLE void foldTopLevel(bool fold);
    Q_INVOKABLE void unfoldAll();
    /** Unfolds whatever hides the character at @p position. */
    Q_INVOKABLE void reveal(int position);

    /** The line that is shown at @p y: the last one that is not folded away and begins at or above it. */
    Q_INVOKABLE int lineAtY(qreal y) const;

    /**
     * For the time the width of a long text keeps changing, as while the border of its panel
     * is dragged: only the lines from @p firstLine on that hold about @p characters, and at
     * least @p shownLines that are not folded away, are left in the layout, so that wrapping
     * them anew for every step costs next to nothing.
     * Everything else takes no room and no time until endWindow().
     *
     * Returns how far up the first of these lines moves by that, which is the height of
     * what was above it: the view has to be moved down by as much for the text to stay
     * where it is on screen.
     */
    Q_INVOKABLE qreal beginWindow(int firstLine, int characters, int shownLines);
    /**
     * Puts all lines back into the layout. They are not laid out by this; the next change
     * of the width does that.
     */
    Q_INVOKABLE void endWindow();

Q_SIGNALS:
    void documentChanged();
    void decoratedChanged();
    void foldableChanged();
    void topOffsetChanged();
    void tabWidthChanged();
    void numbersChanged();
    void layoutChanged();

private:
    struct Region {
        int open = 0; ///< position of the opening bracket
        int close = 0;
        int startLine = 0;
        int endLine = 0;
        bool topLevel = false;
        QString closing; ///< what stands for the hidden lines, e.g. "}" or "},"
    };

    QTextDocument *textDocument() const;
    void readText(const QString &text);
    void readIndents();
    void findRegions();
    bool isFolded(const Region &region) const;
    QList<bool> hiddenLines() const;
    void applyHiddenLines(QTextDocument *doc, const QList<bool> &hidden);
    int base() const;
    int lineEnd(int line) const;
    void readTops(QTextDocument *doc, int firstLine, int lastLine);
    void applyHangingIndent(QTextDocument *doc, bool enabled);
    QString labelFor(int line) const;

    QPointer<QQuickTextDocument> m_document;
    bool m_decorated = true;
    bool m_foldable = false;
    qreal m_topOffset = 0;
    int m_tabWidth = 4;
    QList<int> m_numberedLines;
    QList<int> m_lineNumbers;

    QString m_text;
    QList<int> m_lineStarts;
    QHash<int, Region> m_regions; ///< by first line
    QList<int> m_folds; ///< first lines of the folded regions
    QList<int> m_indents; ///< leading columns of every line
    QList<qreal> m_tops;
    qreal m_bottom = 0;
    // See beginWindow().
    bool m_window = false;
    int m_windowFirst = 0;
    int m_windowLast = 0;
    // See setSource().
    bool m_partial = false;
    bool m_held = false; ///< whether the document holds lines of the source yet
    int m_firstLine = 0;
    int m_lastLine = -1;
    qreal m_top = 0;
    int m_revision = 0;
    bool m_formatted = false; ///< whether the document carries formatting of ours
    bool m_working = false;
};
