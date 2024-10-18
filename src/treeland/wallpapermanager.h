// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QQmlEngine>
#include <QQuickItem>

namespace Waylib::Server {
class WOutput;
class WOutputItem;
} // namespace Waylib::Server

class WallpaperImage;
class WallpaperController;

class WallpaperManager : public QObject
{
    Q_OBJECT
    explicit WallpaperManager(QQuickItem *parent = nullptr);
    ~WallpaperManager();

public:
    static WallpaperManager *instance();

private:
    friend class WallpaperImage;
    void add(WallpaperImage *proxy, Waylib::Server::WOutputItem *outputItem);
    void remove(WallpaperImage *proxy);
    void remove(Waylib::Server::WOutputItem *outputItem);

private:
    friend class WallpaperImage;
    friend class WallpaperController;
    bool isLocked(const WallpaperController *controller) const;
    bool isSelfLocked(const WallpaperController *controller) const;

private:
    friend class WallpaperController;
    WallpaperImage *get(Waylib::Server::WOutputItem *outputItem) const;
    WallpaperImage *get(Waylib::Server::WOutput *output) const;
    void setLock(WallpaperController *controller, bool lock);

private:
    QMap<Waylib::Server::WOutputItem *, WallpaperImage *> m_proxys;
    QList<WallpaperController *> m_proxyLockList;
};
