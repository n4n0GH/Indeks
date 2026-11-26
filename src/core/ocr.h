// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QImage>
#include <QList>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>

#include <functional>

namespace Indeks::Ocr
{
/** Whether the tesseract program is installed. */
bool isAvailable();

/** Language codes tesseract has trained data for, e.g. "eng", "deu". */
QStringList installedLanguages();

/** Reduces a "+"-separated wish list to the installed languages; falls back to what is there. */
QString usableLanguages(const QString &wanted);

/**
 * A word tesseract read, and where it stands. With these, text can be selected on a scan and
 * a place that was found can be pointed at, although the scan is a picture.
 */
struct Word {
    QString text;
    /** Where it stands, as shares of the width and the height of the image (0 to 1). */
    QRectF box;
    /** Which line of the image it is on, counted from 0 in the order of reading. */
    int line = 0;
    /** The first word of a paragraph: an empty line stands before it in the text. */
    bool paragraph = false;
};
/** The words of an image, or of a page, in the order of reading. */
using Page = QList<Word>;

/** What tesseract made of one image. */
struct Recognized {
    QString text;
    Page words;
    bool ok = false; ///< false if the image could not be read or tesseract failed on it
};

/**
 * The words in what tesseract writes about an image of @p size when it is asked for "tsv":
 * a table with a row for every block, paragraph, line and word, and where each stands.
 */
Page parseLayout(const QByteArray &tsv, const QSize &size);

/** Words as text: a line for each line of the image, an empty one between paragraphs. */
QString textOf(const Page &words);

/** How many tesseract processes run at once unless told otherwise: most of the cores. */
int defaultParallelism();

/** Whether an image of this size is too small to hold text worth looking for. */
bool isTooSmallForText(const QSize &size);

/**
 * Recognises text in @p count images, several at a time. Reading text is by far the slowest
 * part of indexing and each image is a job of its own, so this is where the cores go.
 *
 * @p image is asked for the images one by one, in order and on the calling thread, each just
 * before it is needed. @p parallel is the number of tesseract processes to run at once (0:
 * defaultParallelism()). @p progress hears how many images are done.
 * Blocks until all is done or the calling thread's task is aborted (see Task).
 */
QList<Recognized> recognizeAll(int count,
                               const std::function<QImage(int)> &image,
                               const QString &languages,
                               int parallel = 0,
                               const std::function<void(int)> &progress = {});

/** Recognises text in @p image. @p ok is false if tesseract could not be run. */
QString recognize(const QImage &image, const QString &languages, bool *ok = nullptr);

/** Reads an image file of any format Qt can read, as recognition wants it. */
QImage readImageFile(const QString &path);

/**
 * Recognises text in an image file of any format Qt can read. @p layout, if given, gets
 * the words and where they stand: one page.
 */
QString recognizeImageFile(const QString &path, const QString &languages, bool *ok = nullptr, QList<Page> *layout = nullptr);

/**
 * Renders up to @p maxPages pages of a PDF and recognises the text on them, @p parallel at a
 * time. @p layout, if given, gets the words of each of those pages and where they stand.
 */
QString recognizePdfFile(const QString &path, const QString &languages, int maxPages = 200, int parallel = 0, bool *ok = nullptr, QList<Page> *layout = nullptr);
}
