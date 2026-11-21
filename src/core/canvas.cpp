// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "canvas.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace Indeks
{

namespace
{
constexpr qint64 MaxCanvasFileSize = 32LL * 1024 * 1024;
constexpr qreal BoundsMargin = 60.0;
constexpr qreal MinimumNodeSize = 10.0;
constexpr qreal MaximumCoordinate = 1.0e7; // keeps a broken file from asking for an absurd surface
}

Canvas readCanvas(const QString &path)
{
    Canvas canvas;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxCanvasFileSize) {
        return canvas;
    }
    const QByteArray data = file.readAll();
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    // Obsidian writes an empty file for a canvas nothing was put on yet.
    if (!document.isObject() && !data.trimmed().isEmpty()) {
        return canvas;
    }
    const QJsonObject root = document.object();

    auto bounded = [](double value) {
        return qBound(-MaximumCoordinate, value, MaximumCoordinate);
    };

    const QJsonArray nodes = root.value("nodes"_L1).toArray();
    for (const QJsonValue &value : nodes) {
        const QJsonObject object = value.toObject();
        CanvasNode node;
        node.id = object.value("id"_L1).toString();
        const QString type = object.value("type"_L1).toString();
        node.type = type == "file"_L1 ? CanvasNode::File : type == "link"_L1 ? CanvasNode::Link : type == "group"_L1 ? CanvasNode::Group : CanvasNode::Text;
        node.rect = QRectF(bounded(object.value("x"_L1).toDouble()),
                           bounded(object.value("y"_L1).toDouble()),
                           qBound(MinimumNodeSize, object.value("width"_L1).toDouble(), MaximumCoordinate),
                           qBound(MinimumNodeSize, object.value("height"_L1).toDouble(), MaximumCoordinate));
        node.color = object.value("color"_L1).toString();
        node.text = object.value("text"_L1).toString();
        node.file = object.value("file"_L1).toString();
        node.url = object.value("url"_L1).toString();
        node.label = object.value("label"_L1).toString();
        if (!node.id.isEmpty()) {
            canvas.nodes.append(node);
            canvas.bounds = canvas.bounds.isNull() ? node.rect : canvas.bounds.united(node.rect);
        }
    }

    const QJsonArray edges = root.value("edges"_L1).toArray();
    for (const QJsonValue &value : edges) {
        const QJsonObject object = value.toObject();
        CanvasEdge edge;
        edge.id = object.value("id"_L1).toString();
        edge.fromNode = object.value("fromNode"_L1).toString();
        edge.toNode = object.value("toNode"_L1).toString();
        edge.fromSide = object.value("fromSide"_L1).toString();
        edge.toSide = object.value("toSide"_L1).toString();
        // By default a connection has an arrow where it arrives and none where it starts.
        edge.arrowAtStart = object.value("fromEnd"_L1).toString() == "arrow"_L1;
        edge.arrowAtEnd = object.value("toEnd"_L1).toString(u"arrow"_s) == "arrow"_L1;
        edge.color = object.value("color"_L1).toString();
        edge.label = object.value("label"_L1).toString();
        if (!edge.fromNode.isEmpty() && !edge.toNode.isEmpty()) {
            canvas.edges.append(edge);
        }
    }

    if (canvas.bounds.isNull()) {
        canvas.bounds = QRectF(0, 0, 400, 300);
    }
    canvas.bounds.adjust(-BoundsMargin, -BoundsMargin, BoundsMargin, BoundsMargin);
    canvas.valid = true;
    return canvas;
}

QString canvasText(const Canvas &canvas)
{
    QStringList parts;
    for (const CanvasNode &node : canvas.nodes) {
        parts << node.label << node.text << node.file << node.url;
    }
    for (const CanvasEdge &edge : canvas.edges) {
        parts << edge.label;
    }
    parts.removeAll(QString());
    return parts.join(u'\n');
}

QString vaultRoot(const QString &directory)
{
    QDir dir(directory);
    for (int depth = 0; depth < 64; ++depth) {
        if (QFileInfo(dir.filePath(u".obsidian"_s)).isDir()) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QDir(directory).absolutePath();
}

QString resolveCanvasFile(const QString &canvasDirectory, const QString &file)
{
    if (canvasDirectory.isEmpty() || file.isEmpty() || QDir::isAbsolutePath(file)) {
        return {};
    }
    // Obsidian stores paths relative to the vault; a canvas outside of any vault is its own root.
    const QStringList roots = {vaultRoot(canvasDirectory), QDir(canvasDirectory).absolutePath()};
    for (const QString &root : roots) {
        const QString candidate = QDir::cleanPath(root + u'/' + file);
        if (candidate.startsWith(root + u'/') && QFileInfo(candidate).isFile()) {
            return candidate;
        }
    }
    return {};
}

}
