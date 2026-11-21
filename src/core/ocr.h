// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QImage>
#include <QList>
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

/** What tesseract made of one image. */
struct Recognized {
    QString text;
    bool ok = false; ///< false if the image could not be read or tesseract failed on it
};

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

/** Recognises text in an image file of any format Qt can read. */
QString recognizeImageFile(const QString &path, const QString &languages, bool *ok = nullptr);

/** Renders up to @p maxPages pages of a PDF and recognises the text on them, @p parallel at a time. */
QString recognizePdfFile(const QString &path, const QString &languages, int maxPages = 200, int parallel = 0, bool *ok = nullptr);
}
