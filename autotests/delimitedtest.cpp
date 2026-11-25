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
