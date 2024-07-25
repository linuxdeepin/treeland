// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapermanager.h"

#include "wallpaperproxy.h"

#include <woutput.h>

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

void WallpaperManager::add(WallpaperProxy *proxy, Waylib::Server::WOutput *output)
{
    Q_ASSERT(m_proxys.find(output) == m_proxys.end());
    m_proxys[output] = proxy;

    connect(proxy, &WallpaperProxy::destroyed, [this, proxy] {
        remove(proxy);
    });
}

void WallpaperManager::remove(WallpaperProxy *proxy)
{
    m_proxys.remove(m_proxys.key(proxy));
}

void WallpaperManager::remove(Waylib::Server::WOutput *output)
{
    m_proxys.remove(output);
}

WallpaperProxy *WallpaperManager::get(Waylib::Server::WOutput *output)
{
    return m_proxys[output];
}
