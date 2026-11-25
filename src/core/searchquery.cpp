// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "searchquery.h"

#include "fingerprint.h"

#include "types.h"

#include <QDate>
#include <QDateTime>
#include <QRegularExpression>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace Indeks
{

namespace
{
struct Token {
    QString key; // empty for free text
    QString value;
    bool quoted = false;
    bool negated = false;
    bool isOr = false;
};

QList<Token> tokenize(const QString &input)
{
    QList<Token> tokens;
    const qsizetype n = input.size();
    qsizetype i = 0;
    while (i < n) {
        if (input[i].isSpace()) {
            ++i;
            continue;
        }
        Token token;
        if (input[i] == u'-' && i + 1 < n && !input[i + 1].isSpace()) {
            token.negated = true;
            ++i;
        }
        QString current;
        bool sawQuote = false;
        while (i < n && !input[i].isSpace()) {
            const QChar c = input[i];
            if (c == u'"') {
                sawQuote = true;
                ++i;
                while (i < n && input[i] != u'"') {
                    current += input[i++];
                }
                ++i; // closing quote
            } else if (c == u':' && token.key.isEmpty() && !sawQuote && !current.isEmpty()
                       && std::all_of(current.cbegin(), current.cend(), [](QChar ch) {
                              return ch.isLetter();
                          })) {
                token.key = current.toLower();
                current.clear();
                ++i;
            } else {
                current += c;
                ++i;
            }
        }
        token.value = current;
        token.quoted = sawQuote;
        if (token.key.isEmpty() && !token.quoted && !token.negated && token.value == "OR"_L1) {
            token.isOr = true;
        }
        if (token.isOr || !token.value.isEmpty()) {
            tokens.append(token);
        }
    }
    return tokens;
}

QString likePattern(const QString &value)
{
    QString escaped = value;
    escaped.replace(u'\\', "\\\\"_L1).replace(u'%', "\\%"_L1).replace(u'_', "\\_"_L1);
    return u'%' + escaped + u'%';
}

/**
 * LIKE pattern for a file name pattern with the wildcards of file managers: "*" for any number
 * of characters, "?" for one. Unlike likePattern() it has to match the whole name.
 */
QString globPattern(const QString &value)
{
    QString pattern;
    pattern.reserve(value.size() + 4);
    for (const QChar c : value) {
        if (c == u'*') {
            pattern += u'%';
        } else if (c == u'?') {
            pattern += u'_';
        } else if (c == u'%' || c == u'_' || c == u'\\') {
            pattern += u'\\';
            pattern += c;
        } else {
            pattern += c;
        }
    }
    return pattern;
}

bool hasWildcard(const QString &value)
{
    return value.contains(u'*') || value.contains(u'?');
}

bool hasWordCharacter(const QString &value)
{
    return std::any_of(value.cbegin(), value.cend(), [](QChar c) {
        return c.isLetterOrNumber();
    });
}

/** FTS5 string for one word (prefix match) or phrase (exact). */
QString ftsTerm(const QString &value, bool phrase)
{
    QString escaped = value;
    escaped.replace(u'"', "\"\""_L1);
    return phrase ? QString(u'"' + escaped + u'"') : QString(u'"' + escaped + "\"*"_L1);
}

/** Parses "7d", "today", ">2026-01-31" and so on into a condition on an epoch-seconds column. */
bool dateCondition(const QString &column, const QString &rawValue, QString &sql, QVariantList &binds)
{
    QString value = rawValue.toLower();
    QString op;
    if (value.startsWith(">="_L1) || value.startsWith("<="_L1)) {
        op = value.left(2);
        value = value.mid(2);
    } else if (value.startsWith(u'>') || value.startsWith(u'<')) {
        op = value.left(1);
        value = value.mid(1);
    }

    const QDate today = QDate::currentDate();
    QDate from;
    QDate to; // exclusive
    if (value == "today"_L1) {
        from = today;
        to = today.addDays(1);
    } else if (value == "yesterday"_L1) {
        from = today.addDays(-1);
        to = today;
    } else if (value == "week"_L1) {
        from = today.addDays(-7);
    } else if (value == "month"_L1) {
        from = today.addMonths(-1);
    } else if (value == "year"_L1) {
        from = today.addYears(-1);
    } else if (static const QRegularExpression recent(u"^(\\d+)(min|h)$"_s); recent.match(value).hasMatch()) {
        // The last so many minutes or hours count from now, not from midnight.
        const auto match = recent.match(value);
        const qint64 seconds = match.captured(1).toLongLong() * (match.captured(2) == "h"_L1 ? 3600 : 60);
        sql = column + (op.startsWith(u'<') ? " < ?"_L1 : " >= ?"_L1);
        binds = {QDateTime::currentSecsSinceEpoch() - seconds};
        return true;
    } else {
        static const QRegularExpression relative(u"^(\\d+)([dwmy])$"_s);
        const auto match = relative.match(value);
        if (match.hasMatch()) {
            const int amount = match.captured(1).toInt();
            const QChar unit = match.captured(2).at(0);
            from = unit == u'd' ? today.addDays(-amount)
                : unit == u'w'  ? today.addDays(-7 * amount)
                : unit == u'm'  ? today.addMonths(-amount)
                                : today.addYears(-amount);
        } else {
            QDate date = QDate::fromString(value, Qt::ISODate);
            if (date.isValid()) {
                from = date;
                to = date.addDays(1);
            } else if (date = QDate::fromString(value, u"yyyy-MM"_s); date.isValid()) {
                from = date;
                to = date.addMonths(1);
            } else if (date = QDate::fromString(value, u"yyyy"_s); date.isValid()) {
                from = date;
                to = date.addYears(1);
            } else {
                return false;
            }
        }
    }

    const qint64 fromSecs = from.startOfDay().toSecsSinceEpoch();
    const qint64 toSecs = to.isValid() ? to.startOfDay().toSecsSinceEpoch() : 0;
    if (op == ">"_L1) {
        // "after that day" for absolute dates, "older than" makes no sense for relative ones
        sql = column + " >= ?"_L1;
        binds = {to.isValid() ? toSecs : fromSecs};
    } else if (op == ">="_L1) {
        sql = column + " >= ?"_L1;
        binds = {fromSecs};
    } else if (op == "<"_L1) {
        sql = column + " < ?"_L1;
        binds = {fromSecs};
    } else if (op == "<="_L1) {
        sql = column + " < ?"_L1;
        binds = {to.isValid() ? toSecs : fromSecs};
    } else if (to.isValid()) {
        sql = u"(%1 >= ? AND %1 < ?)"_s.arg(column);
        binds = {fromSecs, toSecs};
    } else {
        sql = column + " >= ?"_L1;
        binds = {fromSecs};
    }
    return true;
}

bool sizeCondition(const QString &rawValue, QString &sql, QVariantList &binds)
{
    static const QRegularExpression pattern(u"^(>=|<=|>|<|=)?(\\d+(?:[.,]\\d+)?)\\s*([kmgt]?)i?b?$"_s, QRegularExpression::CaseInsensitiveOption);
    const auto match = pattern.match(rawValue);
    if (!match.hasMatch()) {
        return false;
    }
    double number = match.captured(2).replace(u',', u'.').toDouble();
    const QString unit = match.captured(3).toLower();
    const int power = unit.isEmpty() ? 0 : int(u"kmgt"_s.indexOf(unit)) + 1;
    for (int i = 0; i < power; ++i) {
        number *= 1024.0;
    }
    const QString op = match.captured(1).isEmpty() ? u">="_s : match.captured(1);
    sql = "d.size "_L1 + op + " ?"_L1;
    binds = {qint64(number)};
    return true;
}

int kindFromName(const QString &name)
{
    static const QHash<QString, Kind> kinds = {
        {u"text"_s, Kind::PlainText},
        {u"plain"_s, Kind::PlainText},
        {u"rich"_s, Kind::RichText},
        {u"richtext"_s, Kind::RichText},
        {u"formatted"_s, Kind::RichText},
        {u"document"_s, Kind::RichText},
        {u"pdf"_s, Kind::Pdf},
        {u"image"_s, Kind::Image},
        {u"picture"_s, Kind::Image},
        {u"email"_s, Kind::Email},
        {u"mail"_s, Kind::Email},
        {u"canvas"_s, Kind::Canvas},
        {u"website"_s, Kind::Website},
        {u"web"_s, Kind::Website},
        {u"other"_s, Kind::Other},
    };
    const auto it = kinds.constFind(name.toLower());
    return it == kinds.constEnd() ? -1 : int(*it);
}
}

SearchQuery::SearchQuery(const QString &query)
{
    const QList<Token> tokens = tokenize(query);

    // "a b OR c" means a AND (b OR c): OR binds tighter than the implicit AND.
    QStringList andParts;
    QStringList orParts;
    QStringList ftsTerms;
    bool pendingOr = false;

    auto flushOr = [&] {
        if (orParts.size() == 1) {
            andParts.append(orParts.first());
        } else if (orParts.size() > 1) {
            andParts.append(u'(' + orParts.join(" OR "_L1) + u')');
        }
        orParts.clear();
    };

    QVariantList allBinds;

    for (const Token &token : tokens) {
        if (token.isOr) {
            pendingOr = !orParts.isEmpty();
            continue;
        }
        Clause clause = compileToken(token.key, token.value, token.quoted);
        if (!clause.valid) {
            // Unknown key: treat the whole token as text so that "12:30" still finds something.
            clause = compileText(QString(), token.key + u':' + token.value, true);
        }
        if (!clause.valid) {
            continue;
        }
        // Words looked for in the text are what previews highlight and snippets show, whether
        // they were typed plainly or as content:word.
        if ((token.key.isEmpty() || token.key == "content"_L1) && !token.negated) {
            m_terms.append(token.value);
            if (hasWordCharacter(token.value)) {
                ftsTerms.append(ftsTerm(token.value, token.quoted || token.value.contains(u' ')));
            }
        }
        m_reachesInside = m_reachesInside || (clause.narrow && !token.negated);
        const QString sql = token.negated ? "NOT "_L1 + clause.sql : clause.sql;
        if (!pendingOr) {
            flushOr();
        }
        orParts.append(sql);
        allBinds.append(clause.binds);
        pendingOr = false;
    }
    flushOr();

    if (!andParts.isEmpty()) {
        m_where = andParts.join(" AND "_L1);
        m_binds = allBinds;
        m_empty = false;
    }
    m_fts = ftsTerms.join(" OR "_L1);
}

SearchQuery::Clause SearchQuery::compileText(const QString &column, const QString &value, bool quoted)
{
    Clause clause;
    if (value.isEmpty()) {
        return clause;
    }
    const bool phrase = quoted || value.contains(u' ');
    const bool searchable = hasWordCharacter(value);

    if (column == "name"_L1) {
        clause.sql = u"d.name LIKE ? ESCAPE '\\'"_s;
        clause.binds = {hasWildcard(value) ? globPattern(value) : likePattern(value)};
    } else if (!column.isEmpty()) {
        if (!searchable) {
            return clause;
        }
        clause.sql = u"d.id IN (SELECT rowid FROM doc_fts WHERE doc_fts MATCH ?)"_s;
        clause.binds = {QString(column + " : "_L1 + ftsTerm(value, phrase))};
    } else if (searchable) {
        // The full text index only finds whole words and prefixes; LIKE adds substring matches on names.
        clause.sql = u"(d.id IN (SELECT rowid FROM doc_fts WHERE doc_fts MATCH ?) OR d.name LIKE ? ESCAPE '\\')"_s;
        clause.binds = {ftsTerm(value, phrase), likePattern(value)};
    } else {
        clause.sql = u"d.name LIKE ? ESCAPE '\\'"_s;
        clause.binds = {likePattern(value)};
    }
    clause.valid = true;
    clause.narrow = true;
    return clause;
}

SearchQuery::Clause SearchQuery::compileToken(const QString &key, const QString &value, bool quoted)
{
    Clause clause;
    auto set = [&clause](const QString &sql, const QVariantList &binds = {}) {
        clause.sql = sql;
        clause.binds = binds;
        clause.valid = true;
    };
    // The same for what a document carries of its own, which also finds the parts of others.
    auto setNarrow = [&clause, &set](const QString &sql, const QVariantList &binds = {}) {
        set(sql, binds);
        clause.narrow = true;
    };

    if (key.isEmpty()) {
        return compileText(QString(), value, quoted);
    }
    if (key == "name"_L1 || key == "content"_L1) {
        return compileText(key, value, quoted);
    }
    if (key == "note"_L1 || key == "notes"_L1) {
        return compileText(u"notes"_s, value, quoted);
    }

    const QString lower = value.toLower();
    if (key == "tag"_L1) {
        if (lower == "none"_L1) {
            set(u"NOT EXISTS (SELECT 1 FROM document_tags dt WHERE dt.doc_id = d.id)"_s);
        } else {
            setNarrow(u"EXISTS (SELECT 1 FROM document_tags dt JOIN tags t ON t.id = dt.tag_id WHERE dt.doc_id = d.id AND t.name = ? COLLATE NOCASE)"_s, {value});
        }
    } else if (key == "tagid"_L1) {
        setNarrow(u"EXISTS (SELECT 1 FROM document_tags dt WHERE dt.doc_id = d.id AND dt.tag_id = ?)"_s, {value.toLongLong()});
    } else if (key == "group"_L1) {
        if (lower == "none"_L1) {
            set(u"NOT EXISTS (SELECT 1 FROM document_folders df WHERE df.doc_id = d.id)"_s);
        } else {
            setNarrow(u"EXISTS (SELECT 1 FROM document_folders df JOIN folders f ON f.id = df.folder_id WHERE df.doc_id = d.id AND f.name = ? COLLATE NOCASE)"_s,
                      {value});
        }
    } else if (key == "groupid"_L1) {
        setNarrow(u"EXISTS (SELECT 1 FROM document_folders df WHERE df.doc_id = d.id AND df.folder_id = ?)"_s, {value.toLongLong()});
    } else if (key == "source"_L1) {
        set(u"d.source_id IN (SELECT s.id FROM sources s WHERE s.name = ? COLLATE NOCASE)"_s, {value});
    } else if (key == "similar"_L1) {
        // Documents that look like the one with this number; with a tilde before the number,
        // roughly like it. Where the document has no fingerprint, none do.
        const bool roughly = value.startsWith(u'~');
        bool isNumber = false;
        const qint64 reference = QStringView(value).mid(roughly ? 1 : 0).toLongLong(&isNumber);
        if (isNumber) {
            set(u"EXISTS (SELECT 1 FROM fingerprints f JOIN fingerprints r ON r.doc_id = ? WHERE f.doc_id = d.id AND %1)"_s.arg(Fingerprint::sqlSimilar(roughly)), {reference});
        }
    } else if (key == "partof"_L1) {
        // What a document holds, e.g. the messages of a mailbox, by the document's number.
        setNarrow(u"d.parent_id = ?"_s, {value.toLongLong()});
    } else if (key == "sourceid"_L1) {
        set(u"d.source_id = ?"_s, {value.toLongLong()});
    } else if (key == "kind"_L1 || key == "type"_L1) {
        const int kind = kindFromName(value);
        if (kind >= 0) {
            set(u"d.kind = ?"_s, {kind});
        }
    } else if (key == "ext"_L1) {
        QString pattern = lower;
        pattern.replace(u'\\', "\\\\"_L1).replace(u'%', "\\%"_L1).replace(u'_', "\\_"_L1);
        set(u"d.name LIKE ? ESCAPE '\\'"_s, {QString("%."_L1 + pattern)});
    } else if (key == "path"_L1 || key == "folder"_L1) {
        // Part of the folder a document is in. Addresses are stored percent-encoded, the file
        // name at their end is not part of the folder.
        const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(value, "/:@"));
        QString pattern = encoded;
        pattern.replace(u'\\', "\\\\"_L1).replace(u'%', "\\%"_L1).replace(u'_', "\\_"_L1);
        set(u"d.url LIKE ? ESCAPE '\\'"_s, {QString(u'%' + pattern + "%/%"_L1)});
    } else if (key == "mime"_L1) {
        set(u"d.mime LIKE ? ESCAPE '\\'"_s, {likePattern(lower)});
    } else if (key == "added"_L1 || key == "modified"_L1) {
        QString sql;
        QVariantList binds;
        if (dateCondition(key == "added"_L1 ? u"d.added"_s : u"d.mtime"_s, value, sql, binds)) {
            set(sql, binds);
        }
    } else if (key == "size"_L1) {
        QString sql;
        QVariantList binds;
        if (sizeCondition(value, sql, binds)) {
            set(sql, binds);
        }
    } else if (key == "is"_L1 || key == "in"_L1) {
        if (lower == "inbox"_L1) {
            // A message is only in the inbox if it was put there, which is something of its own.
            setNarrow(u"d.inbox = 1"_s);
        } else if (lower == "annotated"_L1) {
            setNarrow(u"(d.comment <> '' OR EXISTS (SELECT 1 FROM annotations a WHERE a.doc_id = d.id))"_s);
        } else if (lower == "pinned"_L1 || lower == "flagged"_L1) {
            // "Flagged" is what pinned documents were called once; searches saved then still work.
            setNarrow(u"d.flagged = 1"_s);
        } else if (lower == "linked"_L1) {
            setNarrow(u"EXISTS (SELECT 1 FROM document_links l WHERE l.a_id = d.id OR l.b_id = d.id)"_s);
        } else if (lower == "unfiled"_L1) {
            set(u"NOT EXISTS (SELECT 1 FROM document_folders df WHERE df.doc_id = d.id)"_s);
        } else if (lower == "untagged"_L1) {
            set(u"NOT EXISTS (SELECT 1 FROM document_tags dt WHERE dt.doc_id = d.id)"_s);
        } else if (lower == "duplicate"_L1 || lower == "duplicates"_L1) {
            set(u"(d.hash <> '' AND d.hash IN (SELECT hash FROM documents WHERE trashed = 0 AND hash <> '' GROUP BY hash HAVING COUNT(*) > 1))"_s);
        } else if (lower == "missing"_L1) {
            setNarrow(u"d.missing <> 0"_s);
        } else if (lower == "local"_L1 || lower == "owned"_L1) {
            set(u"d.owned = 1"_s);
        } else if (lower == "copied"_L1) {
            set(u"d.local_copy <> ''"_s);
        } else if (lower == "ocr"_L1) {
            set(u"d.ocr = 2"_s);
        } else if (lower == "trashed"_L1 || lower == "trash"_L1) {
            setNarrow(u"d.trashed = 1"_s);
            m_includesTrash = true;
        }
    }
    return clause;
}

SearchQuery SearchQuery::combine(const SearchQuery &a, const SearchQuery &b)
{
    SearchQuery result;
    if (a.m_empty && b.m_empty) {
        return result;
    }
    if (a.m_empty || b.m_empty) {
        const SearchQuery &only = a.m_empty ? b : a;
        result = only;
        return result;
    }
    result.m_where = u'(' + a.m_where + ") AND ("_L1 + b.m_where + u')';
    result.m_binds = a.m_binds + b.m_binds;
    result.m_terms = a.m_terms + b.m_terms;
    result.m_fts = a.m_fts.isEmpty() ? b.m_fts : b.m_fts.isEmpty() ? a.m_fts : a.m_fts + " OR "_L1 + b.m_fts;
    result.m_includesTrash = a.m_includesTrash || b.m_includesTrash;
    result.m_reachesInside = a.m_reachesInside || b.m_reachesInside;
    result.m_empty = false;
    return result;
}

QString SearchQuery::quote(const QString &value)
{
    QString cleaned = value;
    cleaned.remove(u'"');
    return u'"' + cleaned + u'"';
}

}
