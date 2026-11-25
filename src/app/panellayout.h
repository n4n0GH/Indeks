// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

/**
 * Where the views of the two panels are: the sidebar on the left and the side panel on the
 * right.
 *
 * A view is what a tab of a panel leads to: the database, automation, what is known about
 * a document, the to-do list, searching within a document. Each panel has an upper and a
 * lower area, each with a tab bar of its own and the views of its tabs below it. The user
 * says which view is in which area of which panel; this keeps that, and which view each
 * area shows, from one start to the next.
 *
 * The upper area of a panel is the one that is always there: a panel whose views are all
 * in its lower area has them in the upper one instead, and a panel without views is not
 * shown at all.
 */
class PanelLayout : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by App")
    /** Goes up with every change, for whatever depends on what is where. */
    Q_PROPERTY(int revision READ revision NOTIFY changed)
public:
    explicit PanelLayout(QObject *parent = nullptr);

    /** Every view there is, by the name it goes by here: "database", "info", … */
    static QStringList allViews();

    int revision() const
    {
        return m_revision;
    }

    /** The views of an area, in the order of their tabs. @p side is "left" or "right", @p part "top" or "bottom". */
    Q_INVOKABLE QStringList views(const QString &side, const QString &part) const;
    /** The view an area shows; empty for an area without views. */
    Q_INVOKABLE QString current(const QString &side, const QString &part) const;
    /** "left" or "right"; empty for no view of that name. */
    Q_INVOKABLE QString sideOf(const QString &view) const;
    /** "top" or "bottom". */
    Q_INVOKABLE QString partOf(const QString &view) const;
    /** Whether @p view is the one its area shows. */
    Q_INVOKABLE bool isCurrent(const QString &view) const;
    /** Whether @p view can be given the lower area: not while it is the only one above. */
    Q_INVOKABLE bool canSplit(const QString &view) const;

    /** What a view is called on its tab and in commands, its icon, and what its tab says when asked. */
    Q_INVOKABLE QString title(const QString &view) const;
    Q_INVOKABLE QString icon(const QString &view) const;
    Q_INVOKABLE QString toolTip(const QString &view) const;

    /** Where @p view stands among the tabs of its area, from 0; -1 for no such view. */
    Q_INVOKABLE int indexOf(const QString &view) const;
    /** Whether @p view can go @p by tabs further on (or back, if negative) in its area. */
    Q_INVOKABLE bool canShift(const QString &view, int by) const;

    /** Makes @p view the one its area shows. */
    Q_INVOKABLE void setCurrent(const QString &view);
    /**
     * Puts @p view into the area @p side, @p part, before the tab that stands at @p index
     * there now, or after the last one for an @p index of -1, and brings it up. This is
     * what dragging a tab comes to: another place among the tabs it is with, another area,
     * another panel.
     */
    Q_INVOKABLE void place(const QString &view, const QString &side, const QString &part, int index);
    /** Moves @p view @p by tabs further on in its area, or back. */
    Q_INVOKABLE void shift(const QString &view, int by);
    /** Moves @p view to the other panel, as the last tab of its upper area. */
    Q_INVOKABLE void moveAcross(const QString &view);
    /** Moves @p view to the lower area of its panel. */
    Q_INVOKABLE void split(const QString &view);
    /** Moves @p view back to the upper area of its panel. */
    Q_INVOKABLE void merge(const QString &view);
    /** Everything back to where it is on a new installation. */
    Q_INVOKABLE void reset();

Q_SIGNALS:
    void changed();

private:
    struct Area {
        QStringList views;
        QString current;
    };
    // By side (left, right) and part (top, bottom).
    Area m_areas[2][2];
    int m_revision = 0;

    Area *areaOf(const QString &view);
    const Area *areaOf(const QString &view) const;
    const Area *area(const QString &side, const QString &part) const;
    void setDefaults();
    void load();
    /** Sees to it that every view is somewhere, once, and every area shows one of its views. */
    void normalise();
    void store();
    void touched();
};
