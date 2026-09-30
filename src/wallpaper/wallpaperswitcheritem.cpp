// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperswitcheritem.h"

#include "wallpaperitem.h"
#include "seat/helper.h"
#include "workspace/workspacemodel.h"
#include "wallpapershellinterfacev1.h"
#include "wallpapermanager.h"
#include "workspace.h"
#include "shellhandler.h"

#include <woutput.h>

#include <private/qquickitem_p.h>

#include <QPropertyAnimation>
#include <QTimer>

WAYLIB_SERVER_USE_NAMESPACE

class WallpaperSlot : public WallpaperItem
{
public:
    WallpaperSlot(QQuickItem *parent)
        : WallpaperItem(parent, false)
    {
        setLive(true);
    }
    // False only while parked off-screen before its first reveal.
    bool shown = true;
};

WallpaperSwitcherItem::WallpaperSwitcherItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    m_currentSlot = new WallpaperSlot(this);
    QQuickItemPrivate::get(m_currentSlot)->anchors()->setFill(this);

    connect(Helper::instance()->m_wallpaperManager,
            &WallpaperManager::updateWallpaper,
            this,
            &WallpaperSwitcherItem::handleWallpaperUpdate);
    connect(Helper::instance()->shellHandler()->wallpaperShell(),
            &TreelandWallpaperShellInterfaceV1::wallpaperSurfaceAdded,
            this,
            &WallpaperSwitcherItem::handleWallpaperUpdate);
    connect(Helper::instance()->workspace(),
            &Workspace::workspaceAdded,
            this,
            &WallpaperSwitcherItem::handleWorkspaceAdded);
}

WallpaperSwitcherItem::~WallpaperSwitcherItem()
{
    if (m_slideAnim)
        m_slideAnim->stop(); // finish handler still has live slots here
    delete m_oldSlot;
    delete m_currentSlot;
}

WOutput *WallpaperSwitcherItem::output() const
{
    return m_output;
}

void WallpaperSwitcherItem::setOutput(WOutput *output)
{
    if (m_output == output)
        return;

    m_output = output;
    Q_EMIT outputChanged();

    if (m_currentSlot)
        m_currentSlot->setOutput(output);
    if (m_oldSlot)
        m_oldSlot->setOutput(output);
}

WorkspaceModel *WallpaperSwitcherItem::workspace() const
{
    return m_workspace;
}

void WallpaperSwitcherItem::setWorkspace(WorkspaceModel *workspace)
{
    if (m_workspace == workspace)
        return;

    m_workspace = workspace;
    Q_EMIT workspaceChanged();

    if (m_currentSlot)
        m_currentSlot->setWorkspace(workspace);
    if (m_oldSlot)
        m_oldSlot->setWorkspace(workspace);
}

bool WallpaperSwitcherItem::play() const
{
    return m_play;
}

void WallpaperSwitcherItem::setPlay(bool value)
{
    if (m_play == value)
        return;

    m_play = value;
    Q_EMIT playChanged();

    if (m_currentSlot)
        m_currentSlot->setPlay(value);
    if (m_oldSlot)
        m_oldSlot->setPlay(value);
}

QString WallpaperSwitcherItem::source() const
{
    return m_currentSlot ? m_currentSlot->source() : QString();
}

int WallpaperSwitcherItem::transitionDuration() const
{
    return m_transitionDuration;
}

void WallpaperSwitcherItem::setTransitionDuration(int duration)
{
    if (m_transitionDuration == duration)
        return;

    m_transitionDuration = duration;
    Q_EMIT transitionDurationChanged();
}

void WallpaperSwitcherItem::slowDown()
{
    if (m_currentSlot)
        m_currentSlot->slowDown();
}

void WallpaperSwitcherItem::handleWallpaperUpdate()
{
    if (!m_output || !m_currentSlot)
        return;

    auto config = Helper::instance()->m_wallpaperManager->getOutputConfig(m_output->handle());
    QString newSource;

    if (m_workspace) {
        for (const auto &wsConfig : std::as_const(config.workspaces)) {
            if (wsConfig.workspaceId == m_workspace->id()) {
                newSource = wsConfig.desktopWallpaper;
                break;
            }
        }
    }

    if (newSource == m_currentSlot->source())
        return;

    switchToNewSlot();
}

void WallpaperSwitcherItem::handleWorkspaceAdded()
{
    Helper::instance()->m_wallpaperManager->syncAddWorkspace();
    if (m_currentSlot)
        m_currentSlot->updateSurface();
}

void WallpaperSwitcherItem::switchToNewSlot()
{
    auto *newSlot = new WallpaperSlot(this);
    newSlot->setVisible(false);
    newSlot->shown = false;
    newSlot->setOutput(m_output);
    newSlot->setWorkspace(m_workspace);

    if (newSlot->source().isEmpty()) {
        newSlot->deleteLater();
        return;
    }

    // Curtain effect: park the new wallpaper fully off-screen right, then
    // slide it over the old one like a curtain being drawn across.
    newSlot->setWidth(width());
    newSlot->setHeight(height());
    newSlot->setX(width());

    // Settle any in-flight transition first so the screen always keeps a
    // fully visible wallpaper while the next one waits for ready.
    if (m_slideAnim) {
        m_slideAnim->stop(); // finished -> finishSlideIn completes the current slot
        m_slideAnim = nullptr;
    }
    if (m_currentSlot) {
        if (m_currentSlot->shown) {
            if (m_oldSlot)
                m_oldSlot->deleteLater();
            m_oldSlot = m_currentSlot;
        } else {
            m_currentSlot->deleteLater(); // parked, never shown
        }
    }
    m_currentSlot = newSlot;

    Q_EMIT sourceChanged();

    auto *interface = TreelandWallpaperSurfaceInterfaceV1::get(newSlot->source());
    if (interface && interface->wallpaperReady()) {
        startSlideIn(newSlot);
    } else if (interface) {
        connect(interface,
                &TreelandWallpaperSurfaceInterfaceV1::ready,
                this,
                [this, guard = QPointer<WallpaperSlot>(newSlot)]() {
                    if (guard && m_currentSlot == guard.data())
                        startSlideIn(guard);
                },
                Qt::SingleShotConnection);
    }
}

void WallpaperSwitcherItem::startSlideIn(WallpaperSlot *slot)
{
    if (width() <= 0.0 || height() <= 0.0) {
        finishSlideIn(slot);
        return;
    }

    slot->shown = true;
    slot->setVisible(true);
    auto *anim = new QPropertyAnimation(slot, "x");
    anim->setDuration(m_transitionDuration);
    anim->setStartValue(width());
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished, this, [this, guard = QPointer<WallpaperSlot>(slot)]() {
        finishSlideIn(guard);
    });
    m_slideAnim = anim;
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void WallpaperSwitcherItem::finishSlideIn(WallpaperSlot *slot)
{
    if (m_currentSlot != slot)
        return;

    slot->shown = true;
    slot->setVisible(true);
    slot->setX(0);
    QQuickItemPrivate::get(slot)->anchors()->setFill(this);
    if (m_oldSlot) {
        m_oldSlot->deleteLater();
        m_oldSlot = nullptr;
    }
}
