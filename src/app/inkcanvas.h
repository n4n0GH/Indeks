// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QColor>
#include <QPolygonF>
#include <QQuickPaintedItem>

/**
 * A transparent layer for drawing on top of an image or a PDF page.
 *
 * Strokes are kept in coordinates relative to the item's size, so a drawing made at one zoom
 * level fits at every other. They are exchanged as JSON and stored in the index; the document
 * underneath is never modified.
 */
class InkCanvas : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString strokes READ strokes WRITE setStrokes NOTIFY strokesChanged)
    Q_PROPERTY(bool editing READ isEditing WRITE setEditing NOTIFY editingChanged)
    Q_PROPERTY(Tool tool READ tool WRITE setTool NOTIFY toolChanged)
    Q_PROPERTY(QColor penColor READ penColor WRITE setPenColor NOTIFY penColorChanged)
    Q_PROPERTY(qreal penWidth READ penWidth WRITE setPenWidth NOTIFY penWidthChanged)
    Q_PROPERTY(bool empty READ isEmpty NOTIFY strokesChanged)
public:
    enum Tool {
        Pen,
        Highlighter,
        Eraser,
    };
    Q_ENUM(Tool)

    explicit InkCanvas(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    QString strokes() const;
    void setStrokes(const QString &json);
    bool isEditing() const
    {
        return m_editing;
    }
    void setEditing(bool editing);
    Tool tool() const
    {
        return m_tool;
    }
    void setTool(Tool tool);
    QColor penColor() const
    {
        return m_penColor;
    }
    void setPenColor(const QColor &color);
    /** Pen width in logical pixels at a page width of 1000. */
    qreal penWidth() const
    {
        return m_penWidth;
    }
    void setPenWidth(qreal width);
    bool isEmpty() const
    {
        return m_strokes.isEmpty();
    }

    Q_INVOKABLE void undo();
    Q_INVOKABLE void clear();

Q_SIGNALS:
    void strokesChanged();
    void editingChanged();
    void toolChanged();
    void penColorChanged();
    void penWidthChanged();
    /** The user finished a change; time to save strokes(). */
    void edited();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;

private:
    struct Stroke {
        QColor color;
        qreal width = 0.003; ///< relative to the item width
        bool highlighter = false;
        QPolygonF points; ///< relative to the item size
    };
    QPointF normalized(const QPointF &position) const;
    bool eraseAt(const QPointF &point);
    void finishStroke();

    QList<Stroke> m_strokes;
    bool m_editing = false;
    bool m_drawing = false;
    bool m_erased = false;
    Tool m_tool = Pen;
    QColor m_penColor = QColor(0xda, 0x44, 0x53);
    qreal m_penWidth = 3.0;
};
