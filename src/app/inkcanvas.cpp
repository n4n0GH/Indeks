// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "inkcanvas.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>

using namespace Qt::StringLiterals;

namespace
{
constexpr qreal HighlighterOpacity = 0.35;
constexpr qreal HighlighterWidthFactor = 5.0;
constexpr qreal EraserRadius = 0.012; // relative to the item width

qreal rounded(qreal value)
{
    return qRound64(value * 100000.0) / 100000.0;
}
}

InkCanvas::InkCanvas(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setAcceptTouchEvents(false); // touches arrive as mouse events once editing accepts the left button
}

void InkCanvas::paint(QPainter *painter)
{
    const qreal w = width();
    const qreal h = height();
    if (w <= 0 || h <= 0) {
        return;
    }
    painter->setRenderHint(QPainter::Antialiasing);
    for (const Stroke &stroke : std::as_const(m_strokes)) {
        if (stroke.points.isEmpty()) {
            continue;
        }
        QColor color = stroke.color;
        if (stroke.highlighter) {
            color.setAlphaF(HighlighterOpacity);
        }
        QPen pen(color, qMax(1.0, stroke.width * w), Qt::SolidLine, stroke.highlighter ? Qt::FlatCap : Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        if (stroke.points.size() == 1) {
            painter->drawPoint(QPointF(stroke.points.first().x() * w, stroke.points.first().y() * h));
            continue;
        }
        QPainterPath path;
        path.moveTo(stroke.points.first().x() * w, stroke.points.first().y() * h);
        for (qsizetype i = 1; i < stroke.points.size(); ++i) {
            path.lineTo(stroke.points.at(i).x() * w, stroke.points.at(i).y() * h);
        }
        // One path per stroke, so that a translucent highlighter does not darken where it overlaps itself.
        painter->drawPath(path);
    }
}

QString InkCanvas::strokes() const
{
    QJsonArray array;
    for (const Stroke &stroke : m_strokes) {
        QJsonArray points;
        for (const QPointF &point : stroke.points) {
            points.append(rounded(point.x()));
            points.append(rounded(point.y()));
        }
        array.append(QJsonObject{
            {u"color"_s, stroke.color.name(QColor::HexRgb)},
            {u"width"_s, rounded(stroke.width)},
            {u"tool"_s, stroke.highlighter ? u"highlighter"_s : u"pen"_s},
            {u"points"_s, points},
        });
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void InkCanvas::setStrokes(const QString &json)
{
    QList<Stroke> strokes;
    const QJsonArray array = QJsonDocument::fromJson(json.toUtf8()).array();
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        Stroke stroke;
        stroke.color = QColor::fromString(object.value(u"color"_s).toString());
        if (!stroke.color.isValid()) {
            stroke.color = Qt::red;
        }
        stroke.width = qBound(0.0002, object.value(u"width"_s).toDouble(0.003), 0.2);
        stroke.highlighter = object.value(u"tool"_s).toString() == "highlighter"_L1;
        const QJsonArray points = object.value(u"points"_s).toArray();
        for (qsizetype i = 0; i + 1 < points.size(); i += 2) {
            stroke.points.append(QPointF(points.at(i).toDouble(), points.at(i + 1).toDouble()));
        }
        if (!stroke.points.isEmpty()) {
            strokes.append(stroke);
        }
    }
    m_drawing = false;
    m_strokes = strokes;
    Q_EMIT strokesChanged();
    update();
}

void InkCanvas::setEditing(bool editing)
{
    if (m_editing == editing) {
        return;
    }
    m_editing = editing;
    // While not editing, clicks and drags go through to the view underneath.
    setAcceptedMouseButtons(editing ? Qt::LeftButton : Qt::NoButton);
    if (!editing) {
        finishStroke();
    }
    Q_EMIT editingChanged();
}

void InkCanvas::setTool(Tool tool)
{
    if (m_tool != tool) {
        m_tool = tool;
        Q_EMIT toolChanged();
    }
}

void InkCanvas::setPenColor(const QColor &color)
{
    if (m_penColor != color) {
        m_penColor = color;
        Q_EMIT penColorChanged();
    }
}

void InkCanvas::setPenWidth(qreal width)
{
    if (!qFuzzyCompare(m_penWidth, width)) {
        m_penWidth = width;
        Q_EMIT penWidthChanged();
    }
}

void InkCanvas::undo()
{
    if (m_strokes.isEmpty()) {
        return;
    }
    m_strokes.removeLast();
    Q_EMIT strokesChanged();
    Q_EMIT edited();
    update();
}

void InkCanvas::clear()
{
    if (m_strokes.isEmpty()) {
        return;
    }
    m_strokes.clear();
    Q_EMIT strokesChanged();
    Q_EMIT edited();
    update();
}

QPointF InkCanvas::normalized(const QPointF &position) const
{
    return QPointF(qBound(0.0, position.x() / width(), 1.0), qBound(0.0, position.y() / height(), 1.0));
}

bool InkCanvas::eraseAt(const QPointF &point)
{
    // Distances are measured in units of the item width so that the eraser is round on screen.
    const qreal aspect = height() / width();
    auto distanceToSegment = [aspect](const QPointF &p, const QPointF &a, const QPointF &b) {
        const QPointF pa((p.x() - a.x()), (p.y() - a.y()) * aspect);
        const QPointF ba((b.x() - a.x()), (b.y() - a.y()) * aspect);
        const qreal lengthSquared = ba.x() * ba.x() + ba.y() * ba.y();
        const qreal t = lengthSquared > 0 ? qBound(0.0, (pa.x() * ba.x() + pa.y() * ba.y()) / lengthSquared, 1.0) : 0.0;
        return QLineF(pa, ba * t).length();
    };

    bool erased = false;
    for (qsizetype i = m_strokes.size() - 1; i >= 0; --i) {
        const Stroke &stroke = m_strokes.at(i);
        const qreal reach = EraserRadius + stroke.width * (stroke.highlighter ? HighlighterWidthFactor : 1.0) / 2;
        bool hit = stroke.points.size() == 1 && distanceToSegment(point, stroke.points.first(), stroke.points.first()) <= reach;
        for (qsizetype j = 1; j < stroke.points.size() && !hit; ++j) {
            hit = distanceToSegment(point, stroke.points.at(j - 1), stroke.points.at(j)) <= reach;
        }
        if (hit) {
            m_strokes.removeAt(i);
            erased = true;
        }
    }
    if (erased) {
        Q_EMIT strokesChanged();
        update();
    }
    return erased;
}

void InkCanvas::mousePressEvent(QMouseEvent *event)
{
    if (!m_editing || event->button() != Qt::LeftButton || width() <= 0 || height() <= 0) {
        event->ignore();
        return;
    }
    // Without this a surrounding Flickable takes the drag away as soon as it exceeds its threshold.
    setKeepMouseGrab(true);
    m_drawing = true;
    m_erased = false;
    const QPointF point = normalized(event->position());
    if (m_tool == Eraser) {
        m_erased = eraseAt(point);
    } else {
        Stroke stroke;
        stroke.color = m_penColor;
        stroke.highlighter = m_tool == Highlighter;
        stroke.width = m_penWidth / 1000.0 * (stroke.highlighter ? HighlighterWidthFactor : 1.0);
        stroke.points.append(point);
        m_strokes.append(stroke);
        update();
    }
    event->accept();
}

void InkCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_drawing) {
        event->ignore();
        return;
    }
    const QPointF point = normalized(event->position());
    if (m_tool == Eraser) {
        m_erased = eraseAt(point) || m_erased;
    } else if (!m_strokes.isEmpty()) {
        QPolygonF &points = m_strokes.last().points;
        // Skip moves too small to see; keeps the stored drawing compact.
        const QPointF last = points.last();
        if (qAbs(point.x() - last.x()) * width() + qAbs(point.y() - last.y()) * height() >= 1.5) {
            points.append(point);
            update();
        }
    }
    event->accept();
}

void InkCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    finishStroke();
    event->accept();
}

void InkCanvas::mouseUngrabEvent()
{
    finishStroke();
}

void InkCanvas::finishStroke()
{
    if (!m_drawing) {
        return;
    }
    m_drawing = false;
    setKeepMouseGrab(false);
    if (m_tool != Eraser) {
        Q_EMIT strokesChanged();
        Q_EMIT edited();
    } else if (m_erased) {
        Q_EMIT edited();
    }
}

#include "moc_inkcanvas.cpp"
