// SPDX-FileCopyrightText: 2026 n4n0
// SPDX-License-Identifier: GPL-2.0-or-later

#include "panellayout.h"

#include "indekssettings.h"

#include <KLocalizedString>

using namespace Qt::StringLiterals;

namespace
{
enum Side { Left = 0, Right = 1 };
enum Part { Top = 0, Bottom = 1 };

// Where a view is on a new installation, and where one that the settings do not know of
// yet turns up: a view that a later version brings.
struct Home {
    QLatin1StringView view;
    Side side;
    Part part;
};
constexpr Home homes[] = {
    {"database"_L1, Left, Top}, {"automation"_L1, Left, Top}, {"info"_L1, Right, Top}, {"find"_L1, Right, Top}, {"todo"_L1, Right, Bottom},
};
}

PanelLayout::PanelLayout(QObject *parent)
    : QObject(parent)
{
    load();
}

QStringList PanelLayout::allViews()
{
    QStringList views;
    for (const Home &home : homes) {
        views.append(home.view);
    }
    return views;
}

void PanelLayout::setDefaults()
{
    for (auto &side : m_areas) {
        for (Area &area : side) {
            area = {};
        }
    }
    for (const Home &home : homes) {
        m_areas[home.side][home.part].views.append(home.view);
    }
}

void PanelLayout::load()
{
    const Config *config = Config::self();
    m_areas[Left][Top].views = config->panelLeftTop();
    m_areas[Left][Bottom].views = config->panelLeftBottom();
    m_areas[Right][Top].views = config->panelRightTop();
    m_areas[Right][Bottom].views = config->panelRightBottom();
    const QStringList shown = config->panelCurrent();
    int at = 0;
    for (auto &side : m_areas) {
        for (Area &area : side) {
            area.current = shown.value(at++);
        }
    }
    normalise();
}

void PanelLayout::normalise()
{
    // Every view once, and only views there are.
    const QStringList known = allViews();
    QStringList placed;
    for (auto &side : m_areas) {
        for (Area &area : side) {
            QStringList kept;
            for (const QString &view : std::as_const(area.views)) {
                if (known.contains(view) && !placed.contains(view)) {
                    kept.append(view);
                    placed.append(view);
                }
            }
            area.views = kept;
        }
    }
    for (const Home &home : homes) {
        if (!placed.contains(home.view)) {
            m_areas[home.side][home.part].views.append(home.view);
        }
    }
    for (auto &side : m_areas) {
        // The upper area is the one that is always there.
        if (side[Top].views.isEmpty() && !side[Bottom].views.isEmpty()) {
            side[Top] = side[Bottom];
            side[Bottom] = {};
        }
        for (Area &area : side) {
            if (!area.views.contains(area.current)) {
                area.current = area.views.value(0);
            }
        }
    }
}

void PanelLayout::store()
{
    Config *config = Config::self();
    config->setPanelLeftTop(m_areas[Left][Top].views);
    config->setPanelLeftBottom(m_areas[Left][Bottom].views);
    config->setPanelRightTop(m_areas[Right][Top].views);
    config->setPanelRightBottom(m_areas[Right][Bottom].views);
    QStringList shown;
    for (const auto &side : m_areas) {
        for (const Area &area : side) {
            shown.append(area.current);
        }
    }
    config->setPanelCurrent(shown);
    config->save();
}

void PanelLayout::touched()
{
    normalise();
    store();
    ++m_revision;
    Q_EMIT changed();
}

const PanelLayout::Area *PanelLayout::area(const QString &side, const QString &part) const
{
    return &m_areas[side == "right"_L1 ? Right : Left][part == "bottom"_L1 ? Bottom : Top];
}

const PanelLayout::Area *PanelLayout::areaOf(const QString &view) const
{
    for (const auto &side : m_areas) {
        for (const Area &area : side) {
            if (area.views.contains(view)) {
                return &area;
            }
        }
    }
    return nullptr;
}

PanelLayout::Area *PanelLayout::areaOf(const QString &view)
{
    return const_cast<Area *>(std::as_const(*this).areaOf(view));
}

QStringList PanelLayout::views(const QString &side, const QString &part) const
{
    return area(side, part)->views;
}

QString PanelLayout::current(const QString &side, const QString &part) const
{
    return area(side, part)->current;
}

QString PanelLayout::sideOf(const QString &view) const
{
    const Area *area = areaOf(view);
    if (!area) {
        return {};
    }
    return area == &m_areas[Right][Top] || area == &m_areas[Right][Bottom] ? u"right"_s : u"left"_s;
}

QString PanelLayout::partOf(const QString &view) const
{
    const Area *area = areaOf(view);
    if (!area) {
        return {};
    }
    return area == &m_areas[Left][Bottom] || area == &m_areas[Right][Bottom] ? u"bottom"_s : u"top"_s;
}

bool PanelLayout::isCurrent(const QString &view) const
{
    const Area *area = areaOf(view);
    return area && area->current == view;
}

bool PanelLayout::canSplit(const QString &view) const
{
    // The last view above cannot go below: the area above is the one that is always there.
    const Area *area = areaOf(view);
    return area && partOf(view) == "top"_L1 && area->views.size() > 1;
}

QString PanelLayout::title(const QString &view) const
{
    if (view == "database"_L1) {
        return i18nc("@title:tab the places, groups, tags and locations of the database", "Database");
    }
    if (view == "automation"_L1) {
        return i18nc("@title:tab rules and scripts", "Automation");
    }
    if (view == "info"_L1) {
        return i18nc("@title:tab what is known about the selected document", "Information");
    }
    if (view == "todo"_L1) {
        return i18nc("@title:tab the list of things to do", "To-Do");
    }
    if (view == "find"_L1) {
        return i18nc("@title:tab searching within the selected document", "Find");
    }
    return {};
}

QString PanelLayout::icon(const QString &view) const
{
    if (view == "database"_L1) {
        return u"folder-documents"_s;
    }
    if (view == "automation"_L1) {
        return u"system-run"_s;
    }
    if (view == "info"_L1) {
        return u"documentinfo"_s;
    }
    if (view == "todo"_L1) {
        return u"view-task"_s;
    }
    if (view == "find"_L1) {
        return u"search"_s;
    }
    return {};
}

QString PanelLayout::toolTip(const QString &view) const
{
    if (view == "database"_L1) {
        return i18nc("@info:tooltip", "The documents of the database");
    }
    if (view == "automation"_L1) {
        return i18nc("@info:tooltip", "Rules and scripts");
    }
    if (view == "info"_L1) {
        return i18nc("@info:tooltip", "Information on the selected document");
    }
    if (view == "todo"_L1) {
        return i18nc("@info:tooltip", "To-do list");
    }
    if (view == "find"_L1) {
        return i18nc("@info:tooltip", "Find in the selected document");
    }
    return {};
}

void PanelLayout::setCurrent(const QString &view)
{
    Area *area = areaOf(view);
    if (!area || area->current == view) {
        return;
    }
    area->current = view;
    touched();
}

int PanelLayout::indexOf(const QString &view) const
{
    const Area *area = areaOf(view);
    return area ? int(area->views.indexOf(view)) : -1;
}

bool PanelLayout::canShift(const QString &view, int by) const
{
    const Area *area = areaOf(view);
    if (!area || by == 0) {
        return false;
    }
    const qsizetype to = area->views.indexOf(view) + by;
    return to >= 0 && to < area->views.size();
}

void PanelLayout::place(const QString &view, const QString &side, const QString &part, int index)
{
    Area *from = areaOf(view);
    if (!from) {
        return;
    }
    Area *to = &m_areas[side == "right"_L1 ? Right : Left][part == "bottom"_L1 ? Bottom : Top];
    const qsizetype was = from->views.indexOf(view);
    qsizetype at = index < 0 || index > to->views.size() ? to->views.size() : index;
    if (from == to) {
        // Counted among the tabs as they stand, the view itself among them.
        if (was < at) {
            --at;
        }
        if (at == was) {
            setCurrent(view);
            return;
        }
    }
    from->views.removeAt(was);
    to->views.insert(qMin(at, to->views.size()), view);
    to->current = view;
    touched();
}

void PanelLayout::shift(const QString &view, int by)
{
    if (!canShift(view, by)) {
        return;
    }
    Area *area = areaOf(view);
    const qsizetype was = area->views.indexOf(view);
    area->views.move(was, was + by);
    touched();
}

void PanelLayout::moveAcross(const QString &view)
{
    Area *from = areaOf(view);
    if (!from) {
        return;
    }
    const Side other = sideOf(view) == "left"_L1 ? Right : Left;
    from->views.removeOne(view);
    Area &to = m_areas[other][Top];
    to.views.append(view);
    to.current = view;
    touched();
}

void PanelLayout::split(const QString &view)
{
    if (!canSplit(view)) {
        return;
    }
    const Side side = sideOf(view) == "right"_L1 ? Right : Left;
    m_areas[side][Top].views.removeOne(view);
    m_areas[side][Bottom].views.append(view);
    m_areas[side][Bottom].current = view;
    touched();
}

void PanelLayout::merge(const QString &view)
{
    if (partOf(view) != "bottom"_L1) {
        return;
    }
    const Side side = sideOf(view) == "right"_L1 ? Right : Left;
    m_areas[side][Bottom].views.removeOne(view);
    m_areas[side][Top].views.append(view);
    m_areas[side][Top].current = view;
    touched();
}

void PanelLayout::reset()
{
    setDefaults();
    touched();
}

#include "moc_panellayout.cpp"
