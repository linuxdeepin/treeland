// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapermanager.h"

#include "wallpapercontroller.h"
#include "wallpaperimage.h"
#include "common/treelandlogging.h"

#include <woutput.h>
#include <woutputitem.h>

WallpaperManager::WallpaperManager(QQuickItem *parent)
    : QObject(parent)
{
}

WallpaperManager::~WallpaperManager() { }

WallpaperManager *WallpaperManager::instance()
{
    static WallpaperManager *instance = new WallpaperManager();
    return instance;
}

void WallpaperManager::add(WallpaperImage *proxy, WAYLIB_SERVER_NAMESPACE::WOutputItem *outputItem)
{
    Q_ASSERT(m_proxys.find(outputItem) == m_proxys.end());
    m_proxys[outputItem] = proxy;

    connect(proxy, &WallpaperImage::destroyed, [this, proxy] {
        remove(proxy);
    });
}

void WallpaperManager::remove(WallpaperImage *proxy)
{
    m_proxys.remove(m_proxys.key(proxy));
}

void WallpaperManager::remove(WAYLIB_SERVER_NAMESPACE::WOutputItem *outputItem)
{
    m_proxys.remove(outputItem);
}

bool WallpaperManager::isLocked(const WallpaperController *controller) const
{
    if (!controller) {
        return false;
    }

    for (const auto *c : m_proxyLockList) {
        if (c->output() == controller->output()) {
            return true;
        }
    }

    return false;
}

bool WallpaperManager::isSelfLocked(const WallpaperController *controller) const
{
    if (!controller) {
        return false;
    }

    return m_proxyLockList.contains(controller);
}

WallpaperImage *WallpaperManager::get(WAYLIB_SERVER_NAMESPACE::WOutputItem *outputItem) const
{
    if (!outputItem) {
        return nullptr;
    }

    return get(outputItem->output());
}

WallpaperImage *WallpaperManager::get(WAYLIB_SERVER_NAMESPACE::WOutput *output) const
{
    for (auto *proxy : m_proxys.keys()) {
        if (proxy->output() == output) {
            return m_proxys[proxy];
        }
    }

    qCWarning(treelandWallpaper) << "No wallpaper proxy found for output" << output;
    return nullptr;
}

void WallpaperManager::setLock(WallpaperController *controller, bool lock)
{
    if (!controller) {
        return;
    }

    if (lock && !m_proxyLockList.contains(controller)) {
        m_proxyLockList.append(controller);
    } else if (!lock) {
        m_proxyLockList.removeOne(controller);
    }
}
