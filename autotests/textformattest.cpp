// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "textformat.h"

#include <QTest>

using namespace Indeks;
using namespace Qt::StringLiterals;

class TextFormatTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void guesses_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<TextFormat::Guess>("expected");

        using TextFormat::Guess;

        // ------------------------------------------------------------------ plain text
        QTest::newRow("empty") << QString() << Guess::Plain;
        QTest::newRow("prose") << u"Dear Ms. Rutherford,\n\nthank you for your letter, which arrived on Monday.\nI will answer in full next week.\n\nKind regards\n"_s
                               << Guess::Plain;
        // Lines that are kept apart by line breaks must not be run together.
        QTest::newRow("address") << u"Ada Lovelace\n12 St James's Square\nLondon\n"_s << Guess::Plain;
        QTest::newRow("a list with dashes") << u"To buy:\n- bananas\n- oat milk\n- a pineapple\n"_s << Guess::Plain;
        QTest::newRow("settings with comments") << u"# where the files are\n# and how many\npath=/srv/files\ncount=3\n\n# colours\nred=1\n"_s << Guess::Plain;
        QTest::newRow("a script") << u"#!/bin/sh\n# says hello\necho hello\n"_s << Guess::Plain;
        QTest::newRow("one heading and nothing else") << u"# Notes\n\nNothing much happened today.\n"_s << Guess::Plain;
        QTest::newRow("sentences with a comma each") << u"First, we went home.\nThen, we had tea.\nLater, it rained.\n"_s << Guess::Plain;
        QTest::newRow("two rows are no table") << u"a\tb\nc\td\n"_s << Guess::Plain;
        QTest::newRow("rows of different width") << u"a,b,c\nd,e\nf,g,h,i\nj\n"_s << Guess::Plain;
        QTest::newRow("quoted mail") << u"> Are you coming on Friday?\n\nYes, at eight.\n"_s << Guess::Plain;

        // -------------------------------------------------------------------- Markdown
        QTest::newRow("headings of two levels") << u"# Findings\n\nStatus: leads only.\n\n## Method\n\nEvery message was read.\n"_s << Guess::Markdown;
        QTest::newRow("two headings") << u"# One\n\nText.\n\n# Two\n\nMore text.\n"_s << Guess::Markdown;
        QTest::newRow("a heading and a link") << u"# Sources\n\nSee [the manual](https://example.org/manual) for more.\n"_s << Guess::Markdown;
        QTest::newRow("a heading and a list") << u"# Shopping\n\n- bananas\n- oat milk\n"_s << Guess::Markdown;
        QTest::newRow("a heading and bold text") << u"# Status\n\n**Not verified.** Most of it is hearsay.\n"_s << Guess::Markdown;
        QTest::newRow("code in fences") << u"Run it like this:\n\n```\npython3 sweep.py\n```\n"_s << Guess::Markdown;
        QTest::newRow("a table in Markdown") << u"| Name | Town |\n| --- | --- |\n| Ada | London |\n| Linus | Helsinki |\n"_s << Guess::Markdown;
        QTest::newRow("links alone") << u"[One](https://example.org/1) and [two](notes.md) are worth reading.\nSo is [three](https://example.org/3).\n"_s << Guess::Markdown;
        QTest::newRow("things to do") << u"- [ ] call the plumber\n- [x] pay the rent\n"_s << Guess::Markdown;
        QTest::newRow("bold and code") << u"Use `git status` and `git diff` first.\n\n**Never** push without looking.\n"_s << Guess::Markdown;
        // What stands in a fence is code, whatever it looks like.
        QTest::newRow("a comment in a fence") << u"# Setup\n\n```\n# install it\n# then run it\nmake install\n```\n\nDone.\n\n## After\n\nNothing.\n"_s << Guess::Markdown;

        // ---------------------------------------------------------------------- tables
        QTest::newRow("tabs") << u"name\ttown\nAda\tLondon\nLinus\tHelsinki\n"_s << Guess::Table;
        QTest::newRow("commas") << u"name,town,age\nAda,London,36\nLinus,Helsinki,55\n"_s << Guess::Table;
        QTest::newRow("semicolons") << u"name;town;age\r\nAda;London;36\r\nLinus;Helsinki;55\r\n"_s << Guess::Table;
        QTest::newRow("bars") << u"name|town|age\nAda|London|36\nLinus|Helsinki|55\n"_s << Guess::Table;
        QTest::newRow("two columns, enough rows") << u"a,1\nb,2\nc,3\nd,4\ne,5\n"_s << Guess::Table;
        QTest::newRow("two columns, too few rows") << u"a,1\nb,2\nc,3\n"_s << Guess::Plain;
        QTest::newRow("quoted values") << u"name,town,note\n\"Lovelace, Ada\",London,\"first line\nsecond line\"\nLinus,Helsinki,none\n"_s << Guess::Table;
        QTest::newRow("an empty line between rows") << u"a\tb\tc\n\nd\te\tf\ng\th\ti\n"_s << Guess::Table;
        // Bars between spaces, where a value holds a bar itself.
        QTest::newRow("bars between spaces") << u"own | 1 Sep | spiegel|lage\nown | 2 Sep | none\nother | 3 Sep | none\n"_s << Guess::Table;
        // A list of things found, each with its text set in below it.
        QTest::newRow("rows with text below them")
            << u"external | 11 Jun 2015 | ada@example.org | [a term]\n   The text of the first, with commas, and more.\n"
               "own | 2 Sep 2026 | linus@example.org | [another]\n   The text of the second.\n"
               "own | 3 Sep 2026 | grace@example.org | [third]\n   The text of the third.\n"_s
            << Guess::Table;
        QTest::newRow("headed paragraphs are no rows") << u"Shopping:\n   bananas\n   oat milk\nErrands:\n   the parcel\n"_s << Guess::Plain;
        // Only the beginning of a long file is at hand: its last line is cut short.
        QTest::newRow("cut short") << u"a,b,c\nd,e,f\ng,h,i\nj,k,l\nm,n,o\np,q,r\ns,t"_s << Guess::Table;
    }

    void guesses()
    {
        QFETCH(QString, text);
        QFETCH(TextFormat::Guess, expected);
        QCOMPARE(TextFormat::guess(text), expected);
    }
};

QTEST_GUILESS_MAIN(TextFormatTest)

#include "textformattest.moc"
