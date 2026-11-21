// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "types.h"

#include <QMap>
#include <QString>

namespace Indeks
{

struct Extracted {
    QString text;
    /** KFileMetaData property name (e.g. "author") to display value. */
    QMap<QString, QString> metadata;
    int pages = 0;
    /** The file is an image or a scanned PDF, so its text has to come from OCR. */
    bool wantsOcr = false;
};

/**
 * Pulls searchable text and metadata out of a local file. Never modifies the file.
 * Safe to call from any thread.
 */
Extracted extractContent(const QString &path, const QString &mimeType, Kind kind, const IndexOptions &options);

/**
 * What is to be searched and known of one e-mail message, given as the bytes it is made of.
 * The metadata also holds `emailDateSecs`, the time it was sent in seconds since 1970, for
 * whoever stores the message; that is not for showing.
 */
Extracted extractMessage(const QByteArray &raw);

/** Reads a text file, guessing its encoding. Returns at most @p maxBytes of it. */
QString readTextFile(const QString &path, qint64 maxBytes = 16 * 1024 * 1024);

/** Plain text of an RTF document. */
QString rtfToPlainText(const QByteArray &rtf);

/** Content hash used to recognise duplicates and moved files. */
QString hashFile(const QString &path);

}
