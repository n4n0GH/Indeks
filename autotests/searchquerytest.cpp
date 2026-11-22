// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "excerpt.h"
#include "searchquery.h"

#include <QDateTime>
#include <QTest>

using namespace Indeks;
using namespace Qt::StringLiterals;

class SearchQueryTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void emptyQueryMatchesEverything()
    {
        const SearchQuery query(u"   "_s);
        QVERIFY(query.isEmpty());
        QCOMPARE(query.where(), u"1"_s);
        QVERIFY(query.binds().isEmpty());
    }

    void wordsSearchTextAndNames()
    {
        const SearchQuery query(u"tax report"_s);
        QVERIFY(!query.isEmpty());
        QCOMPARE(query.terms(), QStringList({u"tax"_s, u"report"_s}));
        QCOMPARE(query.binds().size(), 4); // per word: one full text term, one name pattern
        QCOMPARE(query.binds().at(0).toString(), u"\"tax\"*"_s);
        QCOMPARE(query.binds().at(1).toString(), u"%tax%"_s);
        QVERIFY(query.where().contains(" AND "_L1));
        QCOMPARE(query.ftsExpression(), u"\"tax\"* OR \"report\"*"_s);
    }

    void phrasesAreExact()
    {
        const SearchQuery query(u"\"annual report\""_s);
        QCOMPARE(query.binds().at(0).toString(), u"\"annual report\""_s);
    }

    void filters()
    {
        const SearchQuery query(u"tag:invoice kind:pdf -is:inbox size:>2M"_s);
        QCOMPARE(query.binds().size(), 3);
        QCOMPARE(query.binds().at(0).toString(), u"invoice"_s);
        QCOMPARE(query.binds().at(1).toInt(), 3);
        QCOMPARE(query.binds().at(2).toLongLong(), 2LL * 1024 * 1024);
        QVERIFY(query.where().contains("NOT d.inbox = 1"_L1));
        QVERIFY(query.terms().isEmpty());
    }

    void fileNamePatterns()
    {
        // Without wildcards a name filter finds the text anywhere in the name …
        QCOMPARE(SearchQuery(u"name:report"_s).binds().at(0).toString(), u"%report%"_s);
        // … with them it is a pattern for the whole name, as in a file manager.
        QCOMPARE(SearchQuery(u"name:*.pdf"_s).binds().at(0).toString(), u"%.pdf"_s);
        QCOMPARE(SearchQuery(u"name:scan_??.png"_s).binds().at(0).toString(), u"scan\\___.png"_s);
        const SearchQuery either(u"name:*.md OR name:*.txt kind:text"_s);
        QVERIFY(either.where().startsWith(u"(d.name LIKE"_s));
        QVERIFY(either.where().contains(u") AND d.kind"_s));
    }

    void folders()
    {
        const SearchQuery query(u"path:\"Tax Returns/2025\""_s);
        QCOMPARE(query.binds().at(0).toString(), u"%Tax\\%20Returns/2025%/%"_s);
        QVERIFY(query.where().contains(u"d.url LIKE"_s));
    }

    void recentHours()
    {
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const SearchQuery query(u"modified:12h added:30min"_s);
        QCOMPARE(query.binds().size(), 2);
        QVERIFY(qAbs(query.binds().at(0).toLongLong() - (now - 12 * 3600)) <= 2);
        QVERIFY(qAbs(query.binds().at(1).toLongLong() - (now - 30 * 60)) <= 2);
        QVERIFY(query.where().contains(u"d.mtime >= ?"_s));
    }

    void contentWordsAreHighlighted()
    {
        const SearchQuery query(u"content:dredger content:\"harbour committee\" name:minutes"_s);
        QCOMPARE(query.terms(), QStringList({u"dredger"_s, u"harbour committee"_s}));
        QCOMPARE(query.ftsExpression(), u"\"dredger\"* OR \"harbour committee\""_s);
    }

    void tellsWhatLooksInsideMailboxes()
    {
        // Places, kinds, sizes and dates list the mailbox; what a message can have of its
        // own finds the message.
        QVERIFY(!SearchQuery(QString()).reachesInside());
        QVERIFY(!SearchQuery(u"kind:email added:7d size:>1M sourceid:3"_s).reachesInside());
        QVERIFY(!SearchQuery(u"is:untagged is:unfiled"_s).reachesInside());
        QVERIFY(SearchQuery(u"invoice"_s).reachesInside());
        QVERIFY(SearchQuery(u"kind:email name:engine"_s).reachesInside());
        QVERIFY(SearchQuery(u"tag:answered"_s).reachesInside());
        QVERIFY(SearchQuery(u"is:pinned"_s).reachesInside());
        QVERIFY(SearchQuery(u"is:linked"_s).reachesInside());
        // Pinned documents were called flagged once; both words find them.
        QCOMPARE(SearchQuery(u"is:flagged"_s).where(), SearchQuery(u"is:pinned"_s).where());
        QVERIFY(SearchQuery(u"partof:12"_s).reachesInside());
        QCOMPARE(SearchQuery(u"partof:12"_s).where(), u"d.parent_id = ?"_s);
        // Not having something is no way to ask for a message.
        QVERIFY(!SearchQuery(u"-tag:answered"_s).reachesInside());
        QVERIFY(SearchQuery::combine(SearchQuery(u"sourceid:3"_s), SearchQuery(u"moth"_s)).reachesInside());
    }

    void excerptsShowTheFirstHit()
    {
        const QString text = u"The quarterly report was late.\nNobody had read the Reports of the year before, and the café stayed closed."_s;
        // From the beginning of a word, whatever the case; every hit in the passage is marked.
        QCOMPARE(excerpt(text, {u"report"_s}), u"The quarterly <b>report</b> was late. Nobody had read the <b>Report</b>s of the year before, and the café stayed closed."_s);
        // Not in the middle of a word.
        QVERIFY(excerpt(text, {u"port"_s}).isEmpty());
        // Accents do not matter, in either direction.
        QVERIFY(excerpt(text, {u"cafe"_s}).contains(u"<b>café</b>"_s));
        QVERIFY(excerpt(u"Resume attached"_s, {u"résumé"_s}).contains(u"<b>Resume</b>"_s));
        // Several words are met one after the other, whatever separates them.
        QVERIFY(excerpt(text, {u"was late"_s}).contains(u"<b>was late</b>"_s));
        QVERIFY(excerpt(text, {u"year before"_s}).contains(u"<b>year before</b>"_s));
        QVERIFY(excerpt(text, {u"report late"_s}).isEmpty());
        // What the text contains is not taken for markup.
        QCOMPARE(excerpt(u"if a < b & c then <b>stop</b>"_s, {u"stop"_s}), u"if a &lt; b &amp; c then &lt;b&gt;<b>stop</b>&lt;/b&gt;"_s);
        // Nothing to look for, nothing found.
        QVERIFY(excerpt(text, {}).isEmpty());
        QVERIFY(excerpt(text, {u"--"_s}).isEmpty());
        QVERIFY(excerpt(QString(), {u"report"_s}).isEmpty());
    }

    void excerptsAreCutOutOfLongTexts()
    {
        QString text;
        for (int i = 0; i < 20000; ++i) {
            text += u"filler words everywhere "_s;
        }
        text += u"and then the needle shows up "_s;
        for (int i = 0; i < 20000; ++i) {
            text += u"more filler after it "_s;
        }
        const QString passage = excerpt(text, {u"needle"_s});
        QVERIFY(passage.contains(u"then the <b>needle</b> shows up"_s));
        QVERIFY(passage.startsWith(u'…'));
        QVERIFY(passage.endsWith(u'…'));
        QVERIFY(passage.size() < 220);
        // Words at the cuts are left whole.
        const QStringList whole = {u"filler"_s, u"words"_s, u"everywhere"_s, u"and"_s, u"more"_s, u"after"_s, u"it"_s};
        QVERIFY(whole.contains(passage.mid(1).section(u' ', 0, 0)));
        QVERIFY(whole.contains(passage.chopped(1).section(u' ', -1)));
    }

    void kinds()
    {
        QCOMPARE(SearchQuery(u"kind:canvas"_s).binds().at(0).toInt(), 6);
        QCOMPARE(SearchQuery(u"kind:email"_s).binds().at(0).toInt(), 5);
        // An unknown kind is searched for as text rather than silently matching everything.
        QVERIFY(SearchQuery(u"kind:hologram"_s).binds().at(0).toString().contains("hologram"_L1));
    }

    void quotedFilterValue()
    {
        const SearchQuery query(u"tag:\"two words\""_s);
        QCOMPARE(query.binds().size(), 1);
        QCOMPARE(query.binds().at(0).toString(), u"two words"_s);
        QCOMPARE(SearchQuery::quote(u"two words"_s), u"\"two words\""_s);
    }

    void orBindsTighterThanAnd()
    {
        const SearchQuery query(u"report kind:pdf OR kind:image"_s);
        QVERIFY(query.where().endsWith("(d.kind = ? OR d.kind = ?)"_L1));
        QCOMPARE(query.binds().size(), 4);
    }

    void trashIsOptIn()
    {
        QVERIFY(!SearchQuery(u"report"_s).includesTrash());
        QVERIFY(SearchQuery(u"is:trashed"_s).includesTrash());
    }

    void unknownKeyIsText()
    {
        // Not a filter at all: a time of day.
        QCOMPARE(SearchQuery(u"12:30"_s).binds().at(0).toString(), u"\"12:30\"*"_s);
        // Looks like a filter but is none we know: search for it literally.
        const SearchQuery query(u"https:example"_s);
        QVERIFY(!query.isEmpty());
        QCOMPARE(query.binds().at(0).toString(), u"\"https:example\""_s);
    }

    void likeWildcardsAreEscaped()
    {
        const SearchQuery query(u"name:100%_done"_s);
        QCOMPARE(query.binds().at(0).toString(), u"%100\\%\\_done%"_s);
    }

    void dates()
    {
        const SearchQuery day(u"added:2026-03-05"_s);
        QCOMPARE(day.binds().size(), 2);
        QCOMPARE(day.binds().at(1).toLongLong() - day.binds().at(0).toLongLong(), 24 * 3600);
        QCOMPARE(SearchQuery(u"modified:7d"_s).binds().size(), 1);
        QVERIFY(SearchQuery(u"added:nonsense"_s).binds().at(0).toString().contains("nonsense"_L1)); // falls back to text
    }

    void combine()
    {
        const SearchQuery combined = SearchQuery::combine(SearchQuery(u"is:inbox"_s), SearchQuery(u"report"_s));
        QCOMPARE(combined.where(), u"(d.inbox = 1) AND ((d.id IN (SELECT rowid FROM doc_fts WHERE doc_fts MATCH ?) OR d.name LIKE ? ESCAPE '\\'))"_s);
        QCOMPARE(combined.binds().size(), 2);
        QCOMPARE(SearchQuery::combine(SearchQuery(), SearchQuery(u"is:inbox"_s)).where(), u"d.inbox = 1"_s);
    }
};

QTEST_GUILESS_MAIN(SearchQueryTest)

#include "searchquerytest.moc"
