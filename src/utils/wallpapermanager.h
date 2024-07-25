// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <woutput.h>

#include <QQmlEngine>
#include <QQuickItem>

namespace Waylib::Server {
class WOutput;
}

class WallpaperProxy;

class WallpaperManager : public QObject
{
    Q_OBJECT
    explicit WallpaperManager(QQuickItem *parent = nullptr);
    ~WallpaperManager();

public:
    static WallpaperManager *instance();

private:
    friend class WallpaperProxy;
    void add(WallpaperProxy *proxy, Waylib::Server::WOutput *output);
    void remove(WallpaperProxy *proxy);
    void remove(Waylib::Server::WOutput *output);

private:
    friend class WallpaperController;
    WallpaperProxy *get(Waylib::Server::WOutput *output);

private:
    QMap<Waylib::Server::WOutput *, WallpaperProxy *> m_proxys;
};
