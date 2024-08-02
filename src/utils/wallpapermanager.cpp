// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapermanager.h"

#include "wallpaperproxy.h"

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

void WallpaperManager::add(WallpaperProxy *proxy, Waylib::Server::WOutputItem *outputItem)
{
    Q_ASSERT(m_proxys.find(outputItem) == m_proxys.end());
    m_proxys[outputItem] = proxy;

    connect(proxy, &WallpaperProxy::destroyed, [this, proxy] {
        remove(proxy);
    });
}

void WallpaperManager::remove(WallpaperProxy *proxy)
{
    m_proxys.remove(m_proxys.key(proxy));
}

void WallpaperManager::remove(Waylib::Server::WOutputItem *outputItem)
{
    m_proxys.remove(outputItem);
}

bool WallpaperManager::isLocked(WallpaperProxy *proxy)
{
    if (!proxy) {
        return false;
    }

    return m_proxyLockList.contains(proxy);
}

WallpaperProxy *WallpaperManager::get(Waylib::Server::WOutputItem *outputItem) const
{
    if (!outputItem) {
        return nullptr;
    }

    return get(outputItem->output());
}

WallpaperProxy *WallpaperManager::get(Waylib::Server::WOutput *output) const
{
    for (auto *proxy : m_proxys.keys()) {
        if (proxy->output() == output) {
            return m_proxys[proxy];
        }
    }

    qWarning() << "no wallpaper proxy for" << output;
    return nullptr;
}

void WallpaperManager::setLock(WallpaperProxy *proxy, bool lock)
{
    if (!proxy) {
        return;
    }

    if (lock && !m_proxyLockList.contains(proxy)) {
        m_proxyLockList.append(proxy);
    } else if (!lock) {
        m_proxyLockList.removeOne(proxy);
    }
}
