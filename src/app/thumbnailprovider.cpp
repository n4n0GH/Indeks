// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "thumbnailprovider.h"

#include "extractor.h"
#include "paths.h"
#include "types.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QPdfDocument>
#include <QRunnable>
#include <QTextOption>

#include <atomic>

using namespace Indeks;
using namespace Qt::StringLiterals;

namespace
{
// Thumbnails are made in a few sizes and scaled to what is asked for: the grid is zoomed
// in steps, and a picture for every step would fill the cache with near copies.
constexpr int edges[] = {128, 256, 512};
// How much of a text is read for its thumbnail, and how old a thumbnail in the cache gets
// without being looked at.
constexpr qint64 TextBytes = 6 * 1024;
constexpr int MaxAgeDays = 90;
// No more of these at a time: the rest of the program wants the processors too.
constexpr int MaxThreads = 3;

int edgeFor(const QSize &requested)
{
    const int wanted = qMax(requested.width(), requested.height());
    for (const int edge : edges) {
        if (wanted <= edge) {
            return edge;
        }
    }
    return edges[std::size(edges) - 1];
}

QString cacheFolder()
{
    const QString folder = Paths::cacheDir() + "/thumbnails"_L1;
    QDir().mkpath(folder);
    return folder;
}

QImage ofPicture(const QString &path, int edge)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid() && (size.width() > edge || size.height() > edge)) {
        reader.setScaledSize(size.scaled(edge, edge, Qt::KeepAspectRatio));
    }
    return reader.read();
}

QImage ofPdf(const QString &path, int edge)
{
    QPdfDocument document;
    if (document.load(path) != QPdfDocument::Error::None || document.pageCount() < 1) {
        return {};
    }
    const QSizeF points = document.pagePointSize(0);
    if (points.isEmpty()) {
        return {};
    }
    const QSize size = points.scaled(edge, edge, Qt::KeepAspectRatio).toSize();
    const QImage page = document.render(0, size);
    if (page.isNull()) {
        return {};
    }
    // Paper is white; the page itself is drawn without a ground.
    QImage sheet(page.size(), QImage::Format_RGB32);
    sheet.fill(Qt::white);
    QPainter painter(&sheet);
    painter.drawImage(0, 0, page);
    return sheet;
}

QImage ofText(const QString &path, int edge)
{
    const QString text = readTextFile(path, TextBytes);
    if (text.trimmed().isEmpty()) {
        return {};
    }
    // A sheet of the proportions of a page, with the beginning of the text on it, small.
    QImage sheet(qRound(edge * 0.76), edge, QImage::Format_RGB32);
    sheet.fill(Qt::white);
    QPainter painter(&sheet);
    QFont font;
    font.setPixelSize(qMax(5, edge / 26));
    painter.setFont(font);
    painter.setPen(QColor(0x30, 0x34, 0x38));
    const int margin = qMax(4, edge / 16);
    QTextOption option(Qt::AlignLeft | Qt::AlignTop);
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    painter.drawText(QRectF(sheet.rect().adjusted(margin, margin, -margin, -margin)), text.left(2000), option);
    return sheet;
}

class Response : public QQuickImageResponse, public QRunnable
{
public:
    Response(const QString &id, const QSize &requestedSize)
        : m_id(id)
        , m_edge(edgeFor(requestedSize))
    {
        setAutoDelete(false); // the engine deletes it, once it has said it is done
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    void cancel() override
    {
        m_cancelled = true;
    }

    void run() override
    {
        if (!m_cancelled) {
            m_image = make();
        }
        Q_EMIT finished();
    }

private:
    QImage make() const
    {
        // kind / modified / size / the path of the file, the latter as it is: it is the rest.
        const QStringList parts = m_id.split(u'/');
        if (parts.size() < 5) {
            return {};
        }
        const int kind = parts.at(1).toInt();
        const QString path = QString::fromUtf8(QByteArray::fromPercentEncoding(m_id.section(u'/', 4).toUtf8()));
        if (!QFileInfo(path).isFile()) {
            return {};
        }
        // The same file, as it was then: its name, when it was changed, how large it is.
        const QByteArray key = QCryptographicHash::hash((path + u'|' + parts.at(2) + u'|' + parts.at(3)).toUtf8(), QCryptographicHash::Sha1).toHex();
        const QString cached = u"%1/%2-%3.png"_s.arg(cacheFolder(), QString::fromLatin1(key)).arg(m_edge);
        if (QFileInfo::exists(cached)) {
            const QImage image(cached);
            if (!image.isNull()) {
                // Looked at: it stays for a while longer.
                QFile file(cached);
                if (file.open(QIODevice::ReadWrite)) {
                    file.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
                }
                return image;
            }
        }
        QImage image;
        if (kind == int(Kind::Image)) {
            image = ofPicture(path, m_edge);
        } else if (kind == int(Kind::Pdf)) {
            image = ofPdf(path, m_edge);
        } else if (kind == int(Kind::PlainText)) {
            image = ofText(path, m_edge);
        }
        if (!image.isNull() && !m_cancelled) {
            image.save(cached, "PNG");
        }
        return image;
    }

    const QString m_id;
    const int m_edge;
    std::atomic_bool m_cancelled = false;
    QImage m_image;
};
}

ThumbnailProvider::ThumbnailProvider()
{
    m_pool.setMaxThreadCount(MaxThreads);
}

ThumbnailProvider::~ThumbnailProvider()
{
    // What waits is not made any more; what is being made is waited for, a moment.
    m_pool.clear();
    m_pool.waitForDone(3000);
}

QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    auto response = new Response(id, requestedSize);
    m_pool.start(response);
    return response;
}

QString ThumbnailProvider::address(qint64 documentId, int kind, const QString &path, qint64 modified, qint64 size)
{
    if (path.isEmpty() || (kind != int(Kind::Image) && kind != int(Kind::Pdf) && kind != int(Kind::PlainText))) {
        return {};
    }
    return u"image://thumbnail/%1/%2/%3/%4/"_s.arg(documentId).arg(kind).arg(modified).arg(size) + QString::fromLatin1(path.toUtf8().toPercentEncoding());
}

void ThumbnailProvider::prune()
{
    const QDateTime oldest = QDateTime::currentDateTime().addDays(-MaxAgeDays);
    const QFileInfoList files = QDir(cacheFolder()).entryInfoList({u"*.png"_s}, QDir::Files);
    for (const QFileInfo &file : files) {
        if (file.lastModified() < oldest) {
            QFile::remove(file.absoluteFilePath());
        }
    }
}
