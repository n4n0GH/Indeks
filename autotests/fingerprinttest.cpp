// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "fingerprint.h"

#include <QImage>
#include <QPainter>
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTest>

using namespace Indeks;
using namespace Qt::StringLiterals;

namespace
{
// A page as a sender writes them: a logo and an address at the top, and below it lines,
// as many as this letter happens to have.
QImage page(int letterhead, int lines, int shift = 0)
{
    QImage image(420, 594, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    if (letterhead == 1) {
        painter.fillRect(30 + shift, 30 + shift, 120, 50, QColor(0x1d, 0x4e, 0x89)); // logo at the left
        for (int i = 0; i < 4; ++i) {
            painter.fillRect(290 + shift, 32 + i * 12 + shift, 100 - i * 12, 5, Qt::darkGray); // address at the right
        }
        painter.fillRect(30 + shift, 100 + shift, 360, 2, Qt::black);
    } else if (letterhead == 2) {
        painter.setBrush(QColor(0xc0, 0x39, 0x2b));
        painter.drawEllipse(180 + shift, 20 + shift, 60, 60); // logo in the middle
        painter.fillRect(110 + shift, 95 + shift, 200, 5, Qt::darkGray);
    } else {
        for (int i = 0; i < 3; ++i) {
            painter.fillRect(30 + shift, 40 + i * 12 + shift, 90, 5, Qt::darkGray); // a sender's lines, no logo
        }
    }
    for (int i = 0; i < lines; ++i) {
        painter.fillRect(30, 170 + i * 14, 300 + (i * 37) % 60, 5, Qt::darkGray);
    }
    return image;
}

// A picture: dark, with shapes; @p which makes it another one.
QImage picture(int which)
{
    QImage image(320, 240, QImage::Format_RGB32);
    QPainter painter(&image);
    QLinearGradient sky(0, 0, which == 1 ? 320 : 0, 240);
    sky.setColorAt(0, which == 1 ? QColor(20, 40, 90) : QColor(120, 60, 20));
    sky.setColorAt(1, which == 1 ? QColor(160, 120, 60) : QColor(10, 30, 20));
    painter.fillRect(image.rect(), sky);
    painter.setPen(Qt::NoPen);
    painter.setBrush(which == 1 ? QColor(240, 220, 120) : QColor(40, 160, 200));
    painter.drawEllipse(which == 1 ? QRect(200, 30, 70, 70) : QRect(40, 120, 110, 90));
    painter.setBrush(QColor(15, 15, 15));
    painter.drawRect(which == 1 ? QRect(0, 180, 320, 60) : QRect(220, 0, 100, 240));
    return image;
}
}

class FingerprintTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pagesOfOneLetterheadAreAlike()
    {
        const Fingerprint three = Fingerprint::ofImage(page(1, 3));
        const Fingerprint thirty = Fingerprint::ofImage(page(1, 30));
        QCOMPARE(three.shape, Fingerprint::Page);
        QCOMPARE(thirty.shape, Fingerprint::Page);
        // How much is written below the head does not matter.
        QVERIFY2(Fingerprint::similar(three, thirty, false), qPrintable(QString::number(Fingerprint::distance(three, thirty))));
        QVERIFY(Fingerprint::similar(three, three, false));
    }

    void pagesOfOtherLetterheadsAreNot()
    {
        const Fingerprint first = Fingerprint::ofImage(page(1, 12));
        const Fingerprint second = Fingerprint::ofImage(page(2, 12));
        const Fingerprint plain = Fingerprint::ofImage(page(3, 12));
        QVERIFY2(!Fingerprint::similar(first, second, true), qPrintable(QString::number(Fingerprint::distance(first, second))));
        QVERIFY2(!Fingerprint::similar(first, plain, true), qPrintable(QString::number(Fingerprint::distance(first, plain))));
        QVERIFY2(!Fingerprint::similar(second, plain, true), qPrintable(QString::number(Fingerprint::distance(second, plain))));
    }

    void aScanThatSitsALittleOffIsStillAlike()
    {
        const Fingerprint straight = Fingerprint::ofImage(page(1, 8));
        const Fingerprint moved = Fingerprint::ofImage(page(1, 20, 3));
        QVERIFY2(Fingerprint::similar(straight, moved, true), qPrintable(QString::number(Fingerprint::distance(straight, moved))));
    }

    void picturesAreComparedAsAWhole()
    {
        const QImage original = picture(1);
        const Fingerprint first = Fingerprint::ofImage(original);
        QCOMPARE(first.shape, Fingerprint::Picture);

        // Smaller, and lighter: the same picture.
        QImage lighter = original.scaled(160, 120, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        for (int y = 0; y < lighter.height(); ++y) {
            for (int x = 0; x < lighter.width(); ++x) {
                lighter.setPixelColor(x, y, lighter.pixelColor(x, y).lighter(125));
            }
        }
        const Fingerprint again = Fingerprint::ofImage(lighter);
        QVERIFY2(Fingerprint::similar(first, again, false), qPrintable(QString::number(Fingerprint::distance(first, again))));

        const Fingerprint other = Fingerprint::ofImage(picture(2));
        QCOMPARE(other.shape, Fingerprint::Picture);
        QVERIFY2(!Fingerprint::similar(first, other, true), qPrintable(QString::number(Fingerprint::distance(first, other))));
    }

    void aPageIsNotComparedWithAPicture()
    {
        const Fingerprint paper = Fingerprint::ofImage(page(1, 5));
        const Fingerprint photo = Fingerprint::ofImage(picture(1));
        QCOMPARE(Fingerprint::distance(paper, photo), -1);
        QVERIFY(!Fingerprint::similar(paper, photo, true));
    }

    void whatHasNothingAtItsTopIsAPicture()
    {
        // A sheet with a drawing in its middle has no head to go by.
        QImage sheet(400, 560, QImage::Format_RGB32);
        sheet.fill(Qt::white);
        {
            QPainter painter(&sheet);
            painter.fillRect(120, 260, 160, 120, Qt::darkBlue);
        }
        QCOMPARE(Fingerprint::ofImage(sheet).shape, Fingerprint::Picture);
        // See-through pictures are looked at on white.
        QImage clear(200, 200, QImage::Format_ARGB32);
        clear.fill(Qt::transparent);
        QImage white(200, 200, QImage::Format_RGB32);
        white.fill(Qt::white);
        QCOMPARE(Fingerprint::ofImage(clear), Fingerprint::ofImage(white));
        QVERIFY(!Fingerprint::ofImage(QImage()).isValid());
    }

    void sqlComparesAsTheProgramDoes()
    {
        // SQLite is made to count bits with sums and shifts; it has to come to what the
        // program counts, for bits of every place in a word.
        QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"fingerprinttest"_s);
        db.setDatabaseName(u":memory:"_s);
        QVERIFY(db.open());
        QSqlQuery query(db);
        QVERIFY(query.exec(u"CREATE TABLE fingerprints (doc_id INTEGER PRIMARY KEY, shape INTEGER, ink INTEGER, b0 INTEGER, b1 INTEGER, b2 INTEGER, b3 INTEGER, "
                           "b4 INTEGER, b5 INTEGER, b6 INTEGER, b7 INTEGER)"_s));
        QRandomGenerator random(7);
        QList<Fingerprint> prints;
        Fingerprint base;
        base.shape = Fingerprint::Page;
        for (quint32 &word : base.bits) {
            word = random.generate();
        }
        base.bits[3] = 0xffffffffu;
        base.bits[5] = 0x80000001u;
        for (int i = 0; i < 400; ++i) {
            Fingerprint print = base;
            print.shape = i % 5 == 4 ? Fingerprint::Picture : Fingerprint::Page;
            // Some bits turned over: few for most, many for some.
            const int flips = i == 0 ? 0 : int(random.bounded(i % 7 == 0 ? 120 : 45));
            for (int flip = 0; flip < flips; ++flip) {
                print.bits[random.bounded(Fingerprint::Words)] ^= quint32(1) << random.bounded(32);
            }
            print.ink = 0;
            for (const quint32 word : print.bits) {
                print.ink += qPopulationCount(word);
            }
            if (i % 11 == 10) {
                print.ink = 12; // a head with little in it
            }
            prints.append(print);
            query.prepare(u"INSERT INTO fingerprints VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s);
            query.addBindValue(i + 1);
            query.addBindValue(int(print.shape));
            query.addBindValue(print.ink);
            for (const quint32 word : print.bits) {
                query.addBindValue(qint64(word));
            }
            QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        }
        for (const bool roughly : {false, true}) {
            for (const int reference : {1, 5, 11, 120}) {
                QSet<int> bySql;
                QVERIFY2(query.exec(u"SELECT f.doc_id FROM fingerprints f JOIN fingerprints r ON r.doc_id = %1 WHERE %2"_s.arg(reference).arg(Fingerprint::sqlSimilar(roughly))),
                         qPrintable(query.lastError().text()));
                while (query.next()) {
                    bySql.insert(query.value(0).toInt());
                }
                QSet<int> byProgram;
                for (int i = 0; i < prints.size(); ++i) {
                    if (Fingerprint::similar(prints.at(i), prints.at(reference - 1), roughly)) {
                        byProgram.insert(i + 1);
                    }
                }
                QCOMPARE(bySql, byProgram);
                QVERIFY(bySql.contains(reference));
                QVERIFY(bySql.size() < prints.size());
                if (reference == 1) {
                    // The one the others were made from has many near it.
                    QVERIFY(bySql.size() > 20);
                }
            }
        }
        db.close();
    }
};

QTEST_GUILESS_MAIN(FingerprintTest)

#include "fingerprinttest.moc"
