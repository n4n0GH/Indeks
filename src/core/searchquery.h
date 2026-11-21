// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>

namespace Indeks
{

/**
 * Compiles the query language used by the search field, saved searches and rules
 * into an SQL condition on the `documents d` table.
 *
 * Words are matched against file names, extracted text and notes. Everything else is a
 * `key:value` filter:
 *
 *   tag:invoice   group:Taxes   kind:pdf   kind:canvas   ext:md   source:Documents
 *   name:report   name:*.pdf   path:invoices   content:"exact phrase"   note:todo
 *   is:inbox  is:annotated  is:pinned  is:unfiled  is:untagged  is:duplicate
 *   is:missing  is:local  is:trashed  is:ocr
 *   added:today  added:7d  added:12h  modified:>2026-01-31  size:>5M
 *
 * A leading minus negates a word or filter, `OR` between two of them matches either.
 */
class SearchQuery
{
public:
    explicit SearchQuery(const QString &query = QString());

    /** SQL boolean expression with positional placeholders; "1" for an empty query. */
    QString where() const
    {
        return m_where;
    }
    QVariantList binds() const
    {
        return m_binds;
    }
    /** FTS5 expression of the free words, for snippets. Empty if the query has none. */
    QString ftsExpression() const
    {
        return m_fts;
    }
    /** The free words and phrases, for highlighting in previews. */
    QStringList terms() const
    {
        return m_terms;
    }
    /** Whether the query asks for trashed documents, which are hidden otherwise. */
    bool includesTrash() const
    {
        return m_includesTrash;
    }
    /**
     * Whether the query also looks inside documents that hold others, at the messages of a
     * mailbox. Those are documents of their own, but there are a great many of them, so they
     * only show up where they are asked for by something of theirs: words, a tag, a group, a
     * flag, notes, or the mailbox they are in (`partof:`). A query that goes by place, kind,
     * size or date alone lists the mailbox and not what is in it; whoever runs a query adds
     * `d.parent_id = 0` for those.
     */
    bool reachesInside() const
    {
        return m_reachesInside;
    }
    bool isEmpty() const
    {
        return m_empty;
    }

    /** ANDs two queries. */
    static SearchQuery combine(const SearchQuery &a, const SearchQuery &b);

    /** Quotes @p value so that it survives as one token, e.g. for `tag:"two words"`. */
    static QString quote(const QString &value);

private:
    struct Clause {
        QString sql;
        QVariantList binds;
        bool valid = false;
        bool narrow = false; ///< asks for something of the document's own; see reachesInside()
    };
    Clause compileToken(const QString &key, const QString &value, bool quoted);
    Clause compileText(const QString &column, const QString &value, bool quoted);

    QString m_where = QStringLiteral("1");
    QVariantList m_binds;
    QString m_fts;
    QStringList m_terms;
    bool m_includesTrash = false;
    bool m_reachesInside = false;
    bool m_empty = true;
};

}
