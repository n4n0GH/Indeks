// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "fingerprint.h"

#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPdfDocument>

#include <vector>

using namespace Qt::StringLiterals;

namespace Indeks
{

namespace
{
// The page is looked at 128 points wide, and as high as it is in proportion, within bounds.
constexpr int Width = 128;
constexpr int LeastHeight = 64;
constexpr int MostHeight = 256;
// A page: at least this share of it is light ...
constexpr int LightLevel = 200;
constexpr double LightShare = 0.6;
// ... and its head, the upper quarter, has something in at least this many fields.
constexpr int HeadColumns = 32;
constexpr int HeadRows = 8;
constexpr int LeastInk = 8;
// A field counts as holding something when it is darker than the average by more than this.
constexpr double InkMargin = 2;
// A picture: 16 rows of 17 fields, each compared with its neighbour.
constexpr int PictureColumns = 16;
constexpr int PictureRows = 16;
constexpr double StepMargin = 1;
// What files are read at: more than the grids need, little enough to be quick.
constexpr int ReadEdge = 512;
constexpr int PageRenderWidth = 256;
constexpr int PageRenderMostHeight = 1024;

// How far apart counts as alike, out of 256 bits. Found by looking at some hundred invoices
// and pictures: pages of one letterhead differ in up to a dozen bits and seldom in twenty,
// pages of different ones in forty and more; a picture in other sizes and cuts differs in
// up to seventeen bits, different pictures in thirty-five and more.
constexpr int PageClose = 20;
constexpr int PageRough = 32;
constexpr int PictureClose = 24;
constexpr int PictureRough = 32;

// The picture in shades of grey on white, at a size.
QImage greyAt(const QImage &image, const QSize &size)
{
    QImage flat(image.size(), QImage::Format_RGB32);
    flat.fill(Qt::white);
    {
        QPainter painter(&flat);
        painter.drawImage(0, 0, image);
    }
    return flat.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_Grayscale8);
}

// How light each field of a grid laid over the first @p height rows of a grey picture is.
std::vector<double> fields(const QImage &grey, int height, int columns, int rows)
{
    std::vector<double> means(size_t(columns) * rows, 255.0);
    for (int row = 0; row < rows; ++row) {
        const int top = row * height / rows;
        const int bottom = qMax(top + 1, (row + 1) * height / rows);
        for (int column = 0; column < columns; ++column) {
            const int left = column * grey.width() / columns;
            const int right = qMax(left + 1, (column + 1) * grey.width() / columns);
            qint64 sum = 0;
            for (int y = top; y < bottom; ++y) {
                const uchar *line = grey.constScanLine(y);
                for (int x = left; x < right; ++x) {
                    sum += line[x];
                }
            }
            means[size_t(row) * columns + column] = double(sum) / double((bottom - top) * (right - left));
        }
    }
    return means;
}

void setBit(Fingerprint &print, int number)
{
    print.bits[number / 32] |= quint32(1) << (number % 32);
    ++print.ink;
}
}

Fingerprint Fingerprint::ofImage(const QImage &image)
{
    Fingerprint print;
    if (image.isNull() || image.width() < 1 || image.height() < 1) {
        return print;
    }
    // As a page first.
    const int height = qBound(LeastHeight, qRound(double(Width) * image.height() / image.width()), MostHeight);
    const QImage page = greyAt(image, QSize(Width, height));
    qint64 light = 0;
    for (int y = 0; y < page.height(); ++y) {
        const uchar *line = page.constScanLine(y);
        for (int x = 0; x < page.width(); ++x) {
            light += line[x] >= LightLevel;
        }
    }
    if (double(light) >= LightShare * page.width() * page.height()) {
        const std::vector<double> head = fields(page, height / 4, HeadColumns, HeadRows);
        double average = 0;
        for (const double field : head) {
            average += field;
        }
        average /= double(head.size());
        for (size_t i = 0; i < head.size(); ++i) {
            if (head[i] < average - InkMargin) {
                setBit(print, int(i));
            }
        }
        if (print.ink >= LeastInk) {
            print.shape = Page;
            return print;
        }
        print = Fingerprint();
    }
    // As a picture: whatever its proportions, the same grid.
    const int columns = PictureColumns + 1;
    const QImage whole = greyAt(image, QSize(columns * 8, PictureRows * 8));
    const std::vector<double> grid = fields(whole, whole.height(), columns, PictureRows);
    for (int row = 0; row < PictureRows; ++row) {
        for (int column = 0; column < PictureColumns; ++column) {
            if (grid[size_t(row) * columns + column + 1] > grid[size_t(row) * columns + column] + StepMargin) {
                setBit(print, row * PictureColumns + column);
            }
        }
    }
    print.shape = Picture;
    return print;
}

Fingerprint Fingerprint::ofFile(const QString &path, Kind kind)
{
    if (kind == Kind::Image) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        if (size.isValid() && (size.width() > ReadEdge || size.height() > ReadEdge)) {
            reader.setScaledSize(size.scaled(ReadEdge, ReadEdge, Qt::KeepAspectRatio));
        }
        return ofImage(reader.read());
    }
    if (kind == Kind::Pdf) {
        QPdfDocument document;
        if (document.load(path) != QPdfDocument::Error::None || document.pageCount() < 1) {
            return {};
        }
        const QSizeF points = document.pagePointSize(0);
        if (points.width() < 1 || points.height() < 1) {
            return {};
        }
        const int height = qBound(1, qRound(PageRenderWidth * points.height() / points.width()), PageRenderMostHeight);
        return ofImage(document.render(0, QSize(PageRenderWidth, height)));
    }
    return {};
}

int Fingerprint::distance(const Fingerprint &a, const Fingerprint &b)
{
    if (a.shape == None || a.shape != b.shape) {
        return -1;
    }
    int differing = 0;
    for (int i = 0; i < Words; ++i) {
        differing += qPopulationCount(a.bits[i] ^ b.bits[i]);
    }
    return differing;
}

int Fingerprint::allowance(Shape shape, int inkA, int inkB, bool roughly)
{
    if (shape == None) {
        return -1;
    }
    // Where there is little to go by, nearly all of it has to be at the same place: a line
    // here and a line there are not the same letterhead, and two pictures that are each of
    // one colour from left to right are not the same picture.
    const int most = shape == Page ? (roughly ? PageRough : PageClose) : (roughly ? PictureRough : PictureClose);
    return qMin(most, (inkA + inkB) / 4);
}

bool Fingerprint::similar(const Fingerprint &a, const Fingerprint &b, bool roughly)
{
    const int differing = distance(a, b);
    return differing >= 0 && differing <= allowance(a.shape, a.ink, b.ink, roughly);
}

QString Fingerprint::sqlSimilar(bool roughly)
{
    // Counting the bits two numbers of 32 bits differ in, with what SQLite has: no XOR and
    // no count, but AND, OR, shifts and sums. (a | b) - (a & b) is a XOR b; the count is the
    // usual one that adds up neighbouring bits, then pairs, then nibbles, then bytes.
    static const QString distance = [] {
        QStringList words;
        for (int i = 0; i < Words; ++i) {
            const QString x = u"((f.b%1 | r.b%1) - (f.b%1 & r.b%1))"_s.arg(i);
            const QString pairs = u"(%1 - ((%1 >> 1) & 1431655765))"_s.arg(x);
            const QString nibbles = u"((%1 & 858993459) + ((%1 >> 2) & 858993459))"_s.arg(pairs);
            const QString bytes = u"((%1 + (%1 >> 4)) & 252645135)"_s.arg(nibbles);
            words += u"(((%1 * 16843009) & 4294967295) >> 24)"_s.arg(bytes);
        }
        return words.join(" + "_L1);
    }();
    return u"f.shape = r.shape AND r.shape > 0 AND (%1) <= MIN(CASE f.shape WHEN 1 THEN %2 ELSE %3 END, (f.ink + r.ink) / 4)"_s.arg(distance)
        .arg(roughly ? PageRough : PageClose)
        .arg(roughly ? PictureRough : PictureClose);
}

}
