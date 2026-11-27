// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperswitcheritem.h"

#include "wallpaperitem.h"
#include "core/waylandserver.h"
#include "dconfig/dconfigmanager.h"
#include "dconfig/effectlevel.h"
#include "lockscreen/lockscreenmanager.h"
#include "workspace/workspacemodel.h"
#include "wallpapershellinterfacev1.h"
#include "wallpapermanager.h"
#include "workspace.h"
#include "shellhandler.h"
#include "treelanduserconfig.hpp"

#ifndef DISABLE_DDM
#include "greeter/usermodel.h"

#include <unistd.h>
#endif

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

    ~WallpaperSlot() override
    {
        if (auto *server = WaylandServer::instance(); server && m_greeterPreview)
            server->lockScreenManager()->updateGreeterWallpapers(this, {});
    }

    void setGreeterPreview(TreelandWallpaperSurfaceInterfaceV1 *interface,
                           TreelandWallpaperInterfaceV1::WallpaperType type)
    {
        m_greeterPreview = true;
        WaylandServer::instance()->lockScreenManager()->updateGreeterWallpapers(this, interface->source(), type);
        setWallpaperSurface(interface);
    }

private:
    bool m_greeterPreview = false;
};

WallpaperSwitcherItem::WallpaperSwitcherItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    auto *server = WaylandServer::instance();
#ifndef DISABLE_DDM
    if (auto *userModel = server->userModel()) {
        connect(userModel, &UserModel::currentUserNameChanged,
                this, &WallpaperSwitcherItem::handleWallpaperUpdate);
    }
#endif
    connect(server, &WaylandServer::currentModeChanged,
            this, &WallpaperSwitcherItem::handleWallpaperUpdate);

    m_currentSlot = new WallpaperSlot(this);
    QQuickItemPrivate::get(m_currentSlot)->anchors()->setFill(this);

    connect(server,
            &WaylandServer::wallpaperChanged,
            this,
            &WallpaperSwitcherItem::handleWallpaperUpdate);
    connect(server->shellHandler()->wallpaperShell(),
            &TreelandWallpaperShellInterfaceV1::wallpaperSurfaceAdded,
            this,
            &WallpaperSwitcherItem::handleWallpaperSurfaceAdded);
    connect(server,
            &WaylandServer::workspaceAdded,
            this,
            &WallpaperSwitcherItem::handleWorkspaceAdded);
}

WallpaperSwitcherItem::~WallpaperSwitcherItem()
{
    if (auto *server = WaylandServer::instance())
        server->lockScreenManager()->updateGreeterWallpapers(this, {});
    delete m_oldSlot;
    delete m_currentSlot;
}

void WallpaperSwitcherItem::componentComplete()
{
    QQuickItem::componentComplete();
    handleWallpaperUpdate();
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
    if (!m_workspace)
        handleWallpaperUpdate();
}

WorkspaceModel *WallpaperSwitcherItem::workspace() const
{
    return m_workspace;
}

void WallpaperSwitcherItem::setWorkspace(WorkspaceModel *workspace)
{
    if (m_workspace == workspace)
        return;

    if (m_workspace && m_workspace->workspaceManager()->wallpaperManager())
        disconnect(m_workspace->workspaceManager()->wallpaperManager(),
                   &WallpaperManager::updateWallpaper, this, &WallpaperSwitcherItem::handleWallpaperUpdate);
    m_workspace = workspace;
    if (m_workspace && m_workspace->workspaceManager()->wallpaperManager())
        connect(m_workspace->workspaceManager()->wallpaperManager(),
                &WallpaperManager::updateWallpaper, this, &WallpaperSwitcherItem::handleWallpaperUpdate);
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
    if (!m_workspace)
        handleWallpaperUpdate();
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
    if (!isComponentComplete() || !m_output || !m_currentSlot)
        return;

#ifndef DISABLE_DDM
    auto *server = WaylandServer::instance();
    const auto *userModel = server->userModel();
    TreelandUserConfig *userConfig = nullptr;
    // The workspace keeps its desktop wallpaper behind the lock screen.
    // Startup and shutdown can also show the lock screen item outside LockScreen mode.
    if (!m_workspace && userModel
        && (server->currentMode() == WaylandServer::CurrentMode::LockScreen || m_play)) {
        const auto user = userModel->currentUser();
        const uint uid = user ? static_cast<uint>(user->UID()) : static_cast<uint>(getuid());
        userConfig = DConfigManager::instance()->userConfig(uid);
    }
    if (m_greeterConfig != userConfig) {
        if (m_greeterConfig)
            disconnect(m_greeterConfig, nullptr, this, nullptr);

        m_greeterConfig = userConfig;
        if (userConfig) {
            connect(userConfig, &TreelandUserConfig::configInitializeSucceed,
                    this, &WallpaperSwitcherItem::handleWallpaperUpdate);
            connect(userConfig, &TreelandUserConfig::configInitializeFailed,
                    this, &WallpaperSwitcherItem::handleWallpaperUpdate);
        } else {
            server->lockScreenManager()->updateGreeterWallpapers(this, {});
        }
    }
    if (userConfig) {
        updateGreeterWallpaper();
        return;
    }
#endif

    if (!m_workspace)
        return;
    auto *manager = m_workspace->workspaceManager()->wallpaperManager();
    if (!manager)
        return;
    const QString newSource = manager->desktopWallpaper(m_output, m_workspace);

    if (newSource == m_currentSlot->source())
        return;

    auto *newSlot = createSlot();
    newSlot->setWorkspace(m_workspace);
    if (newSlot->source().isEmpty()) {
        newSlot->deleteLater();
        return;
    }

    auto *server = WaylandServer::instance();
    auto *config = server ? server->config() : nullptr;
    presentSlot(newSlot,
                m_play && (!server || !config || server->isEffectEnabled(config->wallpaperCrossFade())));
}

void WallpaperSwitcherItem::updateGreeterWallpaper()
{
    if (!configurationReady(m_greeterConfig.data()))
        return;

    auto *server = WaylandServer::instance();
    auto *lockScreenManager = server->lockScreenManager();
    const auto wallpaper = WallpaperManager::currentGreeterWallpaper(*m_greeterConfig, m_output);

    const auto &path = wallpaper.lockscreenWallpaper;
    auto *interface = TreelandWallpaperSurfaceInterfaceV1::get(path);
    if (!interface || !interface->wallpaperReady() || !interface->wSurface()) {
        if (interface && !interface->wallpaperReady()) {
            connect(interface, &TreelandWallpaperSurfaceInterfaceV1::ready,
                    this, &WallpaperSwitcherItem::handleWallpaperUpdate,
                    static_cast<Qt::ConnectionType>(Qt::UniqueConnection | Qt::SingleShotConnection));
        }
        lockScreenManager->updateGreeterWallpapers(this, path, wallpaper.lockScreenWallpapertype);
        return;
    }

    if (path == source() && m_currentSlot->surface() == interface->wSurface()) {
        lockScreenManager->updateGreeterWallpapers(this, {});
        return;
    }

    auto *newSlot = createSlot();
    newSlot->setGreeterPreview(interface, wallpaper.lockScreenWallpapertype);
    newSlot->setPlay(m_play);
    lockScreenManager->updateGreeterWallpapers(this, {});

    auto *config = server->config();
    const bool animated = m_play && m_currentSlot->surface()
        && (!config
            || (server->isEffectEnabled(config->lockscreen())
                && server->isEffectEnabled(config->wallpaperCrossFade())));
    presentSlot(newSlot, animated);
}

void WallpaperSwitcherItem::handleWorkspaceAdded()
{
    if (m_currentSlot)
        m_currentSlot->updateSurface();
}

void WallpaperSwitcherItem::handleWallpaperSurfaceAdded(TreelandWallpaperSurfaceInterfaceV1 *interface)
{
    if (interface->wallpaperReady()) {
        handleWallpaperUpdate();
    } else {
        connect(interface, &TreelandWallpaperSurfaceInterfaceV1::ready,
                this, &WallpaperSwitcherItem::handleWallpaperUpdate,
                static_cast<Qt::ConnectionType>(Qt::UniqueConnection | Qt::SingleShotConnection));
    }
}

WallpaperSlot *WallpaperSwitcherItem::createSlot()
{
    auto *newSlot = new WallpaperSlot(this);
    QQuickItemPrivate::get(newSlot)->anchors()->setFill(this);
    newSlot->setOpacity(0);
    newSlot->setOutput(m_output);
    return newSlot;
}

void WallpaperSwitcherItem::presentSlot(WallpaperSlot *newSlot, bool animated)
{
    m_oldSlot = m_currentSlot;
    m_currentSlot = newSlot;

    Q_EMIT sourceChanged();

    startFadeIn(newSlot, m_oldSlot, animated);
}

void WallpaperSwitcherItem::startFadeOut(WallpaperSlot *slot)
{
    auto *fadeOut = new QPropertyAnimation(slot, "opacity", slot);
    fadeOut->setDuration(m_opacityDuration);
    fadeOut->setStartValue(slot->opacity());
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InOutQuad);
    connect(fadeOut, &QPropertyAnimation::finished, slot, &QObject::deleteLater);
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void WallpaperSwitcherItem::startFadeIn(WallpaperSlot *slot, WallpaperSlot *oldSlot, bool animated)
{
    if (!animated || m_opacityDuration == 0) {
        // The new content is ready: show it and release the previous wallpaper in
        // one step instead of fading.
        slot->setOpacity(1.0);
        if (oldSlot)
            oldSlot->deleteLater();
        return;
    }

    auto *anim = new QPropertyAnimation(slot, "opacity");
    anim->setDuration(m_opacityDuration);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    if (oldSlot && m_workspace) {
        startFadeOut(oldSlot);
    } else if (oldSlot) {
        // Give the incoming wallpaper a head start. Leave an outgoing slot
        // that is still fading in alone until it can cover the desktop.
        connect(anim, &QPropertyAnimation::valueChanged, oldSlot,
                [this, oldSlot, fadeOutStarted = false](const QVariant &value) mutable {
            const qreal opacity = value.toReal();
            if (fadeOutStarted || opacity < 0.5 || (opacity < 1.0 && oldSlot->opacity() < 1.0))
                return;
            fadeOutStarted = true;
            startFadeOut(oldSlot);
        });
        connect(slot, &QObject::destroyed, oldSlot, &QObject::deleteLater);
    }
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
