// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QList>
#include <QRectF>
#include <QString>

namespace Indeks
{

/**
 * A card on an Obsidian canvas. The format is JSON Canvas (https://jsoncanvas.org):
 * a card holds text, refers to a file of the vault, links to a web page, or is a group
 * that frames other cards.
 */
struct CanvasNode {
    enum Type {
        Text,
        File,
        Link,
        Group,
    };
    QString id;
    Type type = Text;
    QRectF rect; ///< in canvas coordinates; these can be negative
    QString color; ///< "1" to "6" for the preset colours, "#rrggbb", or empty
    QString text; ///< Markdown of a text card
    QString file; ///< path of a file card, relative to the vault
    QString url; ///< target of a link card
    QString label; ///< caption of a group
};

/** A connection between two cards. */
struct CanvasEdge {
    QString id;
    QString fromNode;
    QString toNode;
    QString fromSide; ///< "top", "right", "bottom", "left", or empty to pick one
    QString toSide;
    bool arrowAtStart = false;
    bool arrowAtEnd = true;
    QString color;
    QString label;
};

struct Canvas {
    bool valid = false;
    QList<CanvasNode> nodes; ///< in drawing order, bottom first
    QList<CanvasEdge> edges;
    QRectF bounds; ///< what the cards cover, with a margin around it
};

Canvas readCanvas(const QString &path);

/** Everything written on a canvas, for the full text index. */
QString canvasText(const Canvas &canvas);

/**
 * The Obsidian vault a directory belongs to: the nearest directory at or above it that has
 * an .obsidian folder. Paths of file cards are relative to it. Falls back to @p directory.
 */
QString vaultRoot(const QString &directory);

/**
 * Local path of the file a file card points at, or empty if it does not exist or would lie
 * outside the vault. Canvases come from anywhere, so their paths are not trusted.
 */
QString resolveCanvasFile(const QString &canvasDirectory, const QString &file);

}
