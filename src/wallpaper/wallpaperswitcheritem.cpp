// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperswitcheritem.h"

#include "wallpaperitem.h"
#include "core/waylandserver.h"
#include "workspace/workspacemodel.h"
#include "wallpapershellinterfacev1.h"
#include "wallpapermanager.h"
#include "workspace.h"
#include "shellhandler.h"
#include "treelanduserconfig.hpp"

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
};

WallpaperSwitcherItem::WallpaperSwitcherItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    m_currentSlot = new WallpaperSlot(this);
    QQuickItemPrivate::get(m_currentSlot)->anchors()->setFill(this);

    connect(WaylandServer::instance()->m_wallpaperManager,
            &WallpaperManager::updateWallpaper,
            this,
            &WallpaperSwitcherItem::handleWallpaperUpdate);
    connect(WaylandServer::instance()->shellHandler()->wallpaperShell(),
            &TreelandWallpaperShellInterfaceV1::wallpaperSurfaceAdded,
            this,
            &WallpaperSwitcherItem::handleWallpaperUpdate);
    connect(WaylandServer::instance()->workspace(),
            &Workspace::workspaceAdded,
            this,
            &WallpaperSwitcherItem::handleWorkspaceAdded);

    setupAnimationConfig();
}

WallpaperSwitcherItem::~WallpaperSwitcherItem()
{
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

int WallpaperSwitcherItem::opacityDuration() const
{
    return m_opacityDuration;
}

void WallpaperSwitcherItem::setOpacityDuration(int duration)
{
    if (m_opacityDuration == duration)
        return;

    m_opacityDuration = duration;
    Q_EMIT opacityDurationChanged();
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

    auto config = WaylandServer::instance()->m_wallpaperManager->getOutputConfig(m_output->handle());
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
    WaylandServer::instance()->m_wallpaperManager->syncAddWorkspace();
    if (m_currentSlot)
        m_currentSlot->updateSurface();
}

void WallpaperSwitcherItem::switchToNewSlot()
{
    auto *newSlot = new WallpaperSlot(this);
    QQuickItemPrivate::get(newSlot)->anchors()->setFill(this);
    newSlot->setOpacity(0);
    newSlot->setOutput(m_output);
    newSlot->setWorkspace(m_workspace);

    if (newSlot->source().isEmpty()) {
        newSlot->deleteLater();
        return;
    }

    // A previous cross fade is superseded: release its leftover slot now so its
    // finished callback can never touch the slot of this switch.
    if (m_oldSlot) {
        m_oldSlot->deleteLater();
        m_oldSlot = nullptr;
    }

    m_oldSlot = m_currentSlot;
    m_currentSlot = newSlot;

    Q_EMIT sourceChanged();

    if (crossFadeEnabled())
        startFadeOut(m_oldSlot);

    auto *interface = TreelandWallpaperSurfaceInterfaceV1::get(newSlot->source());
    if (interface && interface->wallpaperReady()) {
        startFadeIn(newSlot);
    } else if (interface) {
        // Waiting for the content to load is not an animation: keep it even when
        // the cross fade is disabled.
        connect(interface,
                &TreelandWallpaperSurfaceInterfaceV1::ready,
                this,
                [this, newSlot]() {
                    if (m_currentSlot == newSlot)
                        startFadeIn(newSlot);
                },
                Qt::SingleShotConnection);
    }
}

void WallpaperSwitcherItem::startFadeOut(WallpaperSlot *slot)
{
    if (!slot)
        return;

    auto *fadeOut = new QPropertyAnimation(slot, "opacity");
    fadeOut->setDuration(m_opacityDuration);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InOutQuad);
    connect(fadeOut, &QPropertyAnimation::finished, this, [this, slot]() {
        if (m_oldSlot != slot)
            return;
        m_oldSlot->deleteLater();
        m_oldSlot = nullptr;
    });
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void WallpaperSwitcherItem::startFadeIn(WallpaperSlot *slot)
{
    if (!crossFadeEnabled()) {
        // The new content is ready: show it and release the previous wallpaper in
        // one step instead of fading.
        slot->setOpacity(1.0);
        if (m_oldSlot) {
            m_oldSlot->deleteLater();
            m_oldSlot = nullptr;
        }
        return;
    }

    auto *anim = new QPropertyAnimation(slot, "opacity");
    anim->setDuration(m_opacityDuration);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

bool WallpaperSwitcherItem::crossFadeEnabled() const
{
    auto *server = WaylandServer::instance();
    auto *config = server ? server->config() : nullptr;
    // An uninitialized config is not a valid "disabled" answer.
    if (!config || !config->isInitializeSucceeded())
        return true;
    return config->wallpaperCrossFadeAnimationEnabled();
}

void WallpaperSwitcherItem::setupAnimationConfig()
{
    auto *server = WaylandServer::instance();
    if (!server)
        return;

    QObject::disconnect(m_animationConfigConnection);
    if (auto *config = server->config()) {
        m_animationConfigConnection = connect(config,
                                              &TreelandUserConfig::valueChanged,
                                              this,
                                              &WallpaperSwitcherItem::onAnimationConfigChanged);
    }
    connect(server,
            &WaylandServer::configChanged,
            this,
            &WallpaperSwitcherItem::setupAnimationConfig,
            Qt::UniqueConnection);
}

void WallpaperSwitcherItem::onAnimationConfigChanged()
{
    if (crossFadeEnabled() || !m_currentSlot)
        return;

    // Never drop the old wallpaper before the new content is ready: that would
    // leave an empty desktop until the client maps its surface.
    auto *interface = TreelandWallpaperSurfaceInterfaceV1::get(m_currentSlot->source());
    if (!interface || !interface->wallpaperReady())
        return;

    startFadeIn(m_currentSlot);
}
