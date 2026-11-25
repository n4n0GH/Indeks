// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QQuickAsyncImageProvider>
#include <QThreadPool>

/**
 * Small pictures of documents, for the grid of the document list: `image://thumbnail/…`.
 *
 * A picture is shown as itself, a PDF as its first page, a text as its beginning on a
 * sheet. Whatever else a document is, and whatever cannot be read, has no thumbnail: the
 * grid shows the icon of its type then. Only files on this computer are looked at; nothing
 * is fetched for a thumbnail.
 *
 * The pictures are made in threads of their own, a few at a time, and kept in the cache
 * folder, so that a folder of scans is not rendered anew with every start. What a thumbnail
 * is made of is all in the address the model gives (DocumentModel::ThumbnailRole): the
 * index is not asked from here.
 */
class ThumbnailProvider : public QQuickAsyncImageProvider
{
public:
    ThumbnailProvider();
    ~ThumbnailProvider() override;

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

    /** The address of the thumbnail of a file; empty for a kind of document that has none. */
    static QString address(qint64 documentId, int kind, const QString &path, qint64 modified, qint64 size);
    /** Removes thumbnails that were not looked at for a long time. Call once, at start. */
    static void prune();

private:
    QThreadPool m_pool;
};
