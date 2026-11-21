// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "types.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QSet>

using namespace Qt::StringLiterals;

namespace Indeks
{

bool isContainerMimeType(const QString &mimeType)
{
    return mimeType == "application/mbox"_L1;
}

Kind kindForMimeType(const QString &mimeType)
{
    static const QSet<QString> richTypes = {
        u"text/html"_s,
        u"application/xhtml+xml"_s,
        u"text/rtf"_s,
        u"application/rtf"_s,
        u"application/msword"_s,
        u"application/vnd.oasis.opendocument.text"_s,
        u"application/vnd.openxmlformats-officedocument.wordprocessingml.document"_s,
        u"application/epub+zip"_s,
    };
    static const QSet<QString> mailTypes = {
        u"message/rfc822"_s,
        u"application/mbox"_s,
        u"application/vnd.ms-outlook"_s,
    };

    if (mimeType == "application/pdf"_L1) {
        return Kind::Pdf;
    }
    if (mailTypes.contains(mimeType)) {
        return Kind::Email;
    }
    if (richTypes.contains(mimeType)) {
        return Kind::RichText;
    }
    if (mimeType.startsWith("image/"_L1)) {
        return Kind::Image;
    }
    if (mimeType.startsWith("text/"_L1)) {
        return Kind::PlainText;
    }
    // Source code, JSON, YAML and friends all inherit from text/plain.
    static const QMimeDatabase db;
    const QMimeType type = db.mimeTypeForName(mimeType);
    if (type.isValid() && type.inherits(u"text/plain"_s)) {
        return Kind::PlainText;
    }
    return Kind::Other;
}

Kind kindForFile(const QString &fileName, const QString &mimeType)
{
    // Canvases are JSON as far as the MIME database is concerned.
    if (fileName.endsWith(".canvas"_L1, Qt::CaseInsensitive)) {
        return Kind::Canvas;
    }
    return kindForMimeType(mimeType);
}

QString kindIconName(Kind kind)
{
    switch (kind) {
    case Kind::Canvas:
        return u"view-group"_s;
    case Kind::PlainText:
        return u"text-plain"_s;
    case Kind::RichText:
        return u"x-office-document"_s;
    case Kind::Pdf:
        return u"application-pdf"_s;
    case Kind::Image:
        return u"image-x-generic"_s;
    case Kind::Email:
        return u"mail-message"_s;
    case Kind::Other:
        break;
    }
    return u"unknown"_s;
}

QueueItem queueItemForUrl(const QUrl &url, qint64 size)
{
    QueueItem item;
    item.name = url.fileName();
    item.location = url.isLocalFile() ? QFileInfo(url.toLocalFile()).path()
                                      : url.adjusted(QUrl::RemoveFilename | QUrl::RemoveUserInfo | QUrl::StripTrailingSlash).toDisplayString();
    item.size = size;
    return item;
}

}

#include "moc_types.cpp"
