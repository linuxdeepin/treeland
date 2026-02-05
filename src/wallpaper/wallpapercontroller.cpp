// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapercontroller.h"

#include "wallpaperimage.h"
#include "wallpapermanager.h"

WallpaperController::WallpaperController(QObject *parent)
    : QObject(parent)
    , m_output(nullptr)
{
}

WallpaperController::~WallpaperController()
{
    setLock(false);
}

void WallpaperController::setType(WallpaperType type)
{
    auto *manager = WallpaperManager::instance();
    if (manager->isLocked(this) && !manager->isSelfLocked(this)) {
        return;
    }

    m_type = type;

    updateState();
}

void WallpaperController::setOutput(WAYLIB_SERVER_NAMESPACE::WOutput *output)
{
    if (!output) {
        return;
    }

    if (m_output == output) {
        return;
    }

    m_output = output;

    Q_EMIT outputChanged();
    Q_EMIT proxyChanged();
}

void WallpaperController::updateState()
{
    if (!m_output) {
        return;
    }

    auto *manager = WallpaperManager::instance();
    auto *proxy = manager->get(m_output);

    Q_ASSERT(proxy);

    QString state = "Normal";
    switch (m_type) {
    case Normal:
        break;
    case Scale:
        state = "Scale";
        break;
    }
    proxy->setState(state);

    Q_EMIT proxyChanged();
    Q_EMIT typeChanged();
}

WallpaperImage *WallpaperController::proxy() const
{
    if (!m_output) {
        return nullptr;
    }

    auto *manager = WallpaperManager::instance();
    return manager->get(m_output);
}

void WallpaperController::setLock(bool lock)
{
    auto *manager = WallpaperManager::instance();
    manager->setLock(this, lock);
}

bool WallpaperController::lock() const
{
    auto *manager = WallpaperManager::instance();
    return manager->isSelfLocked(this);
}
