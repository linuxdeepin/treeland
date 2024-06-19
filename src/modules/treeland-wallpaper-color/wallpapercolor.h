// Copyright (C) 2024 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wquickwaylandserver.h>

#include <QQmlEngine>

struct wallpaper_color_manager_v1;
WAYLIB_SERVER_USE_NAMESPACE

class TreelandWallpaperColor : public Waylib::Server::WQuickWaylandServerInterface
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit TreelandWallpaperColor(QObject *parent = nullptr);
    Q_INVOKABLE void updateWallpaperColor(const QString &output, bool isDarkType);

protected:
    WServerInterface *create() override;

private:
    wallpaper_color_manager_v1 *m_handle{ nullptr };
};
