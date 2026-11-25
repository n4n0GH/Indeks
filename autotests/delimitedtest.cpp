// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "delimited.h"

#include <QTest>

using namespace Indeks;
using namespace Qt::StringLiterals;

class DelimitedTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsQuotedCells()
    {
        const QString text = u"Name,City,Balance,Note\r\n"
                             "\"Ada Lovelace\",London,\"1,250.50\",\"Wrote \"\"notes\"\"\"\r\n"
                             "Grace,\"Arlington, VA\",9800,\"First line\nsecond line\"\r\n"
                             "\r\n"
                             "Linus,Helsinki,42,\n"_s;
        const DelimitedTable table(text, DelimitedTable::guessSeparator(text, u"people.csv"_s));
        QCOMPARE(table.separator(), QChar(u','));
        QCOMPARE(table.rowCount(), 4); // the empty line is no row
        QCOMPARE(table.columnCount(), 4);
        QCOMPARE(table.cell(1, 0), u"Ada Lovelace"_s);
        QCOMPARE(table.cell(1, 2), u"1,250.50"_s);
        QCOMPARE(table.cell(1, 3), u"Wrote \"notes\""_s);
        QCOMPARE(table.cell(2, 1), u"Arlington, VA"_s);
        QCOMPARE(table.cell(2, 3), u"First line\nsecond line"_s);
        QCOMPARE(table.cell(3, 3), QString()); // the line ends with a separator
        QCOMPARE(table.cell(3, 7), QString()); // no such cell
        QCOMPARE(table.cell(9, 0), QString());

        QVERIFY(table.startsWithHeader());
        QVERIFY(table.isNumeric(2, 1));
        QVERIFY(!table.isNumeric(1, 1));
        QCOMPARE(table.widthHint(0), 12);
        QCOMPARE(table.widthHint(3), 13); // up to the line break
        QCOMPARE(table.find({u"helsinki"_s}), std::make_pair(3, 1));
        QCOMPARE(table.find({u"nowhere"_s}), std::make_pair(-1, -1));
        QCOMPARE(table.find({u"London"_s}, 2), std::make_pair(-1, -1));

        // Every cell that holds a text, in the order of the rows: "in" is in Arlington, in
        // "First line", in Linus and in Helsinki.
        const QList<std::pair<int, int>> all = table.findAll(u"IN"_s, 1, 100);
        QCOMPARE(all.size(), 4);
        QCOMPARE(all.at(0), std::make_pair(2, 1));
        QCOMPARE(all.at(1), std::make_pair(2, 3));
        QCOMPARE(all.at(2), std::make_pair(3, 0));
        QCOMPARE(all.at(3), std::make_pair(3, 1));
        QCOMPARE(table.findAll(u"in"_s, 1, 3).size(), 3); // no more than asked for
        QCOMPARE(table.findAll(u"in"_s, 3, 100).size(), 2); // from a row on
        QVERIFY(table.findAll(QString(), 0, 100).isEmpty());
        QVERIFY(table.findAll(u"nowhere"_s, 0, 100).isEmpty());
    }

    void readsNumbers_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("isNumber");
        QTest::addColumn<double>("value");

        QTest::newRow("plain") << u"42"_s << true << 42.0;
        QTest::newRow("negative") << u"-17"_s << true << -17.0;
        QTest::newRow("point") << u"3.25"_s << true << 3.25;
        QTest::newRow("comma") << u"3,25"_s << true << 3.25;
        QTest::newRow("groups with commas") << u"1,250.50"_s << true << 1250.5;
        QTest::newRow("groups with points") << u"1.250,50"_s << true << 1250.5;
        QTest::newRow("thousands alone") << u"12,345"_s << true << 12345.0;
        QTest::newRow("several groups") << u"1.234.567"_s << true << 1234567.0;
        QTest::newRow("currency") << u"€ 5"_s << true << 5.0;
        QTest::newRow("percent") << u"12 %"_s << true << 12.0;
        QTest::newRow("power of ten") << u"1.5e3"_s << true << 1500.0;
        QTest::newRow("with room around it") << u"  7  "_s << true << 7.0;
        QTest::newRow("a word") << u"seven"_s << false << 0.0;
        QTest::newRow("a date") << u"2026-11-25"_s << false << 0.0;
        QTest::newRow("empty") << QString() << false << 0.0;
    }

    void readsNumbers()
    {
        QFETCH(QString, text);
        QFETCH(bool, isNumber);
        QFETCH(double, value);
        const std::optional<double> number = DelimitedTable::number(text);
        QCOMPARE(number.has_value(), isNumber);
        if (isNumber) {
            QCOMPARE(*number, value);
        }
    }

    void readsBarsBetweenSpaces()
    {
        // Values with a bar between spaces, as people and programs write lists. A value may
        // hold a bar itself, and quotes are part of it.
        const QString text = u"external | 11 Jun 2015  | ada@example.org | spiegel|lage | [a term]\n"
                             "own | 2 Sep 2026 | linus@example.org | \"quoted\" subject | [another]\n"
                             "own | 3 Sep 2026 | grace@example.org |  | [third]\n"_s;
        const DelimitedTable::Dialect dialect = DelimitedTable::guessDialect(text, u"hits.txt"_s);
        QCOMPARE(dialect.separator, QChar(u'|'));
        QVERIFY(dialect.spaced);
        QVERIFY(!dialect.continued);
        const DelimitedTable table(text, dialect);
        QCOMPARE(table.rowCount(), 3);
        QCOMPARE(table.columnCount(), 5);
        QCOMPARE(table.cell(0, 0), u"external"_s);
        QCOMPARE(table.cell(0, 1), u"11 Jun 2015"_s); // without the room around it
        QCOMPARE(table.cell(0, 3), u"spiegel|lage"_s);
        QCOMPARE(table.cell(0, 4), u"[a term]"_s);
        QCOMPARE(table.cell(1, 3), u"\"quoted\" subject"_s);
        QCOMPARE(table.cell(2, 3), QString());
        QCOMPARE(table.cell(2, 4), u"[third]"_s);
        QVERIFY(DelimitedTable::looksLikeSpacedBars(text));

        // Bars without room are the plain kind, read as before.
        const QString plain = u"a|b|c\n1|2|3\n"_s;
        QVERIFY(!DelimitedTable::guessDialect(plain, u"x.txt"_s).spaced);
        QVERIFY(!DelimitedTable::looksLikeSpacedBars(plain));
        QVERIFY(!DelimitedTable::looksLikeSpacedBars(u"First, we went home.\nThen | we had tea.\nLater, it rained.\n"_s));
    }

    void readsRowsWithTextBelowThem()
    {
        // A list of things found: a row of values, and set in below it the text it is
        // about, which may be full of commas and hold a bar between spaces itself.
        const QString text = u"external | 11 Jun 2015 | ada@example.org | Subject, with a comma | [a term]\n"
                             "   The text of the first, which goes on, and on | and on.\n"
                             "own | 2 Sep 2026 | linus@example.org | Second | [another]\n"
                             "    Two lines\n"
                             "    of text.  \n"
                             "\n"
                             "own | 3 Sep 2026 | grace@example.org | Third | [third]\n"
                             "own | 4 Sep 2026 | alan@example.org | Fourth | [fourth]\n"
                             "   The text of the fourth.\n"_s;
        const DelimitedTable::Dialect dialect = DelimitedTable::guessDialect(text, u"hits.txt"_s);
        QCOMPARE(dialect.separator, QChar(u'|'));
        QVERIFY(dialect.spaced);
        QVERIFY(dialect.continued);
        QVERIFY(DelimitedTable::looksLikeSpacedBars(text));

        const DelimitedTable table(text, dialect);
        QCOMPARE(table.rowCount(), 4);
        QCOMPARE(table.columnCount(), 6); // the text is one more column
        QCOMPARE(table.cell(0, 3), u"Subject, with a comma"_s);
        QCOMPARE(table.cell(0, 5), u"The text of the first, which goes on, and on | and on."_s);
        QCOMPARE(table.cell(1, 0), u"own"_s);
        QCOMPARE(table.cell(1, 5), u"Two lines\n    of text."_s);
        QCOMPARE(table.cell(2, 4), u"[third]"_s);
        QCOMPARE(table.cell(2, 5), QString()); // a row without text below it
        QCOMPARE(table.cell(3, 5), u"The text of the fourth."_s);
        // Every row is one of the things found; none of them names the columns.
        QVERIFY(!table.startsWithHeader());
        QCOMPARE(table.findAll(u"text of"_s, 0, 10).size(), 2);
        QCOMPARE(table.findAll(u"of text"_s, 0, 10).constFirst(), std::make_pair(1, 5));

        // A row that is short of a value still has its text in the column of the texts.
        const DelimitedTable ragged(u"a | b | c\n  first\nd | e\n  second\nf | g | h\n  third\n"_s, DelimitedTable::Dialect{u'|', true, true});
        QCOMPARE(ragged.cell(1, 1), u"e"_s);
        QCOMPARE(ragged.cell(1, 2), QString());
        QCOMPARE(ragged.cell(1, 3), u"second"_s);

        // Lines that are set in do not make a table of a text that is none.
        QVERIFY(!DelimitedTable::looksLikeSpacedBars(u"Shopping:\n   bananas\n   oat milk\nErrands:\n   the parcel\n"_s));
        // And the names of files decide as before.
        QCOMPARE(DelimitedTable::guessDialect(text, u"hits.tsv"_s).separator, QChar(u'\t'));
    }

    void tellsTheSeparator_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("name");
        QTest::addColumn<QChar>("separator");

        QTest::newRow("commas") << u"a,b,c\n1,2,3\n"_s << u"x.csv"_s << QChar(u',');
        QTest::newRow("semicolons with decimal commas") << u"id;wert;einheit\n1;3,5;kg\n2;12,25;kg\n"_s << u"x.csv"_s << QChar(u';');
        QTest::newRow("tabs in a csv") << u"a\tb\n1\t2\n"_s << u"x.csv"_s << QChar(u'\t');
        QTest::newRow("bars") << u"a|b|c\n1|2|3\n"_s << u"x.txt"_s << QChar(u'|');
        QTest::newRow("commas inside quotes do not count") << u"\"a, b\";c\n\"d, e\";f\n"_s << u"x.csv"_s << QChar(u';');
        QTest::newRow("by its name") << u"one, two and three\n"_s << u"notes.tsv"_s << QChar(u'\t');
        QTest::newRow("a single column") << u"alpha\nbeta\n"_s << u"x.csv"_s << QChar(u',');
    }

    void tellsTheSeparator()
    {
        QFETCH(QString, text);
        QFETCH(QString, name);
        QFETCH(QChar, separator);
        QCOMPARE(DelimitedTable::guessSeparator(text, name), separator);
    }

    void tellsHeadersFromData()
    {
        QVERIFY(DelimitedTable(u"sample\tvalue\nA\t1\nB\t2\n"_s, u'\t').startsWithHeader());
        QVERIFY(!DelimitedTable(u"1\t2\n3\t4\n"_s, u'\t').startsWithHeader());
        QVERIFY(!DelimitedTable(u"a\t\nb\tc\n"_s, u'\t').startsWithHeader()); // a name is missing
        QVERIFY(!DelimitedTable(u"only\tone\n"_s, u'\t').startsWithHeader());
        QCOMPARE(DelimitedTable(QString(), u',').rowCount(), 0);
        QCOMPARE(DelimitedTable(u"﻿a,b\n"_s, u',').cell(0, 0), u"a"_s); // without the byte order mark

        QVERIFY(DelimitedTable::looksNumeric(u"-12.5"));
        QVERIFY(DelimitedTable::looksNumeric(u"1.234,56 €"));
        QVERIFY(DelimitedTable::looksNumeric(u"45 %"));
        QVERIFY(DelimitedTable::looksNumeric(u"3e-4"));
        QVERIFY(!DelimitedTable::looksNumeric(u"S000012"));
        QVERIFY(!DelimitedTable::looksNumeric(u"2026-03-02 early"));
        QVERIFY(!DelimitedTable::looksNumeric(u""));

        QVERIFY(DelimitedTable::isDelimitedFile(u"Report.CSV"_s, QString()));
        QVERIFY(DelimitedTable::isDelimitedFile(u"export"_s, u"text/tab-separated-values"_s));
        QVERIFY(!DelimitedTable::isDelimitedFile(u"notes.txt"_s, u"text/plain"_s));
    }

    void rowsOfDifferentLength()
    {
        const DelimitedTable table(u"a;b;c\n1\n1;2;3;4\n"_s, u';');
        QCOMPARE(table.rowCount(), 3);
        QCOMPARE(table.columnCount(), 4);
        QCOMPARE(table.cell(1, 0), u"1"_s);
        QCOMPARE(table.cell(1, 2), QString());
        QCOMPARE(table.cell(2, 3), u"4"_s);
    }
};

QTEST_GUILESS_MAIN(DelimitedTest)

#include "delimitedtest.moc"
