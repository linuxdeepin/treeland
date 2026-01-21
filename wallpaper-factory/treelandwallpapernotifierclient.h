// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-treeland-wallpaper-shell-unstable-v1.h"

#include <QObject>
#include <QScreen>
#include <QQuickView>
#include <QtWaylandClient/QWaylandClientExtension>

class TreelandWallpaperNotifierClientV1
    : public QWaylandClientExtensionTemplate<TreelandWallpaperNotifierClientV1>
    , public QtWayland::treeland_wallpaper_notifier_v1
{
    Q_OBJECT
public:
    explicit TreelandWallpaperNotifierClientV1();
    ~TreelandWallpaperNotifierClientV1() override;

    void instantiate();

protected:
    void treeland_wallpaper_notifier_v1_add(uint32_t source_type, const QString &file_source) override;
    void treeland_wallpaper_notifier_v1_remove(const QString &file_source) override;

private Q_SLOTS:
    void updateAllWallpaperViewSizes();
    void onScreenAdded(QScreen *screen);
    void onScreenRemoved(QScreen *screen);

private:
    QList<QQuickView *> m_windows;
    QSet<QScreen *> m_connectedScreens;
};
