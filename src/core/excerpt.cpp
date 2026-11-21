// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "excerpt.h"

#include <array>

using namespace Qt::StringLiterals;

namespace Indeks
{

namespace
{
// Private-use characters mark the hits so that the text around them can be escaped safely.
constexpr QChar HitStart(0xe000);
constexpr QChar HitEnd(0xe001);

bool isWordCharacter(QChar c)
{
    const char16_t code = c.unicode();
    if (code < 128) {
        return (code >= u'a' && code <= u'z') || (code >= u'A' && code <= u'Z') || (code >= u'0' && code <= u'9');
    }
    return c.isLetterOrNumber();
}

char16_t foldSlowly(char16_t code)
{
    QChar c(code);
    if (c.decompositionTag() == QChar::Canonical) {
        // The letter without its accent comes first in the decomposition.
        const QString parts = c.decomposition();
        if (!parts.isEmpty()) {
            c = parts.at(0);
        }
    }
    return c.toCaseFolded().unicode();
}

/** Lower case and without accents, which is how the full-text index compares letters. */
char16_t fold(QChar c)
{
    const char16_t code = c.unicode();
    if (code < 128) {
        return code >= u'A' && code <= u'Z' ? char16_t(code + 32) : code;
    }
    // Western European text stays within these; anything else is looked up as it comes.
    constexpr char16_t TableEnd = 0x250;
    static const std::array<char16_t, TableEnd - 128> table = [] {
        std::array<char16_t, TableEnd - 128> folded{};
        for (char16_t i = 128; i < TableEnd; ++i) {
            folded[i - 128] = foldSlowly(i);
        }
        return folded;
    }();
    return code < TableEnd ? table[code - 128] : foldSlowly(code);
}

/** A term as the words it is made of, folded. */
using Words = QList<QString>;

Words wordsOf(const QString &term)
{
    Words words;
    QString word;
    for (const QChar c : term) {
        if (isWordCharacter(c)) {
            word.append(QChar(fold(c)));
        } else if (!word.isEmpty()) {
            words.append(word);
            word.clear();
        }
    }
    if (!word.isEmpty()) {
        words.append(word);
    }
    return words;
}

/**
 * Whether @p words stand in @p text from @p position on, which is the beginning of a word.
 * Returns where the hit ends, or -1. All words but the last have to be met in full; the last
 * may go on, as a search for "invoi" finds "invoice".
 */
qsizetype matchAt(QStringView text, qsizetype position, const Words &words)
{
    qsizetype at = position;
    for (qsizetype w = 0; w < words.size(); ++w) {
        const QString &word = words.at(w);
        if (w > 0) {
            // Whatever separates the words: spaces, a hyphen, a line break.
            while (at < text.size() && !isWordCharacter(text.at(at))) {
                ++at;
            }
        }
        if (text.size() - at < word.size()) {
            return -1;
        }
        for (qsizetype i = 0; i < word.size(); ++i) {
            if (fold(text.at(at + i)) != word.at(i).unicode()) {
                return -1;
            }
        }
        at += word.size();
        const bool last = w == words.size() - 1;
        if (!last && at < text.size() && isWordCharacter(text.at(at))) {
            return -1;
        }
    }
    return at;
}

struct Hit {
    qsizetype start = -1;
    qsizetype end = -1;
};

/** The first hit of any of @p terms in @p text at or after @p from. */
Hit nextHit(QStringView text, qsizetype from, const QList<Words> &terms)
{
    bool inWord = from > 0 && isWordCharacter(text.at(from - 1));
    for (qsizetype i = from; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (!isWordCharacter(c)) {
            inWord = false;
            continue;
        }
        if (inWord) {
            continue;
        }
        inWord = true;
        const char16_t first = fold(c);
        Hit best;
        for (const Words &words : terms) {
            if (words.constFirst().at(0).unicode() != first) {
                continue;
            }
            const qsizetype end = matchAt(text, i, words);
            if (end > best.end) {
                best = {i, end};
            }
        }
        if (best.start >= 0) {
            return best;
        }
    }
    return {};
}
}

QString excerpt(const QString &text, const QStringList &terms, int before, int after)
{
    QList<Words> wanted;
    for (const QString &term : terms) {
        const Words words = wordsOf(term);
        if (!words.isEmpty()) {
            wanted.append(words);
        }
    }
    if (wanted.isEmpty() || text.isEmpty()) {
        return {};
    }

    const QStringView all(text);
    const Hit first = nextHit(all, 0, wanted);
    if (first.start < 0) {
        return {};
    }

    // Cut at word boundaries where that does not cost much of the passage.
    qsizetype start = qMax<qsizetype>(0, first.start - before);
    if (start > 0) {
        while (start < first.start && isWordCharacter(all.at(start - 1)) && isWordCharacter(all.at(start))) {
            ++start;
        }
    }
    qsizetype end = qMin(all.size(), first.end + after);
    if (end < all.size()) {
        while (end > first.end && isWordCharacter(all.at(end - 1)) && isWordCharacter(all.at(end))) {
            --end;
        }
    }

    // Should the text use the marker characters itself, they must not be taken for hits.
    QString cleaned = text.mid(start, end - start);
    cleaned.replace(HitStart, u' ').replace(HitEnd, u' ');
    const QStringView passage(cleaned);
    QString marked;
    marked.reserve(passage.size() + 16);
    qsizetype done = 0;
    Hit hit{first.start - start, first.end - start};
    while (hit.start >= 0) {
        marked += passage.mid(done, hit.start - done);
        marked += HitStart;
        marked += passage.mid(hit.start, hit.end - hit.start);
        marked += HitEnd;
        done = hit.end;
        hit = nextHit(passage, done, wanted);
    }
    marked += passage.mid(done);

    QString html = marked.simplified().toHtmlEscaped();
    html.replace(HitStart, "<b>"_L1).replace(HitEnd, "</b>"_L1);
    if (start > 0) {
        html.prepend(u'…');
    }
    if (end < all.size()) {
        html.append(u'…');
    }
    return html;
}

}
