// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperproxy.h"

#include "wallpapermanager.h"

WallpaperProxy::WallpaperProxy(QQuickItem *parent)
    : QQuickItem(parent)
{
}

void WallpaperProxy::setSource(const QString &source)
{
    if (m_source == source) {
        return;
    }

    m_source = source;

    Q_EMIT sourceChanged();
}

void WallpaperProxy::setType(Helper::WallpaperType type)
{
    if (WallpaperManager::instance()->isLocked(this)) {
        return;
    }

    if (m_type == type) {
        return;
    }

    m_type = type;

    Q_EMIT typeChanged();
}

void WallpaperProxy::setOutputItem(Waylib::Server::WOutputItem *output)
{
    if (!output) {
        return;
    }

    if (m_output == output) {
        return;
    }

    m_output = output;

    WallpaperManager::instance()->add(this, m_output);

    Q_EMIT outputItemChanged();
}
