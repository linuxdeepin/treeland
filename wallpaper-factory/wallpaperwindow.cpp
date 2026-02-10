// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperwindow.h"
#include "loggings.h"
#include "qwaylandwallpapershellintegration_p.h"

#include <QMap>
#include <QPlatformSurfaceEvent>

#include <QtWaylandClient/private/qwaylandwindow_p.h>

static QMap<QWindow *, WallpaperWindow *> s_map;

class WallpaperWindowPrivate
{
public:
    WallpaperWindowPrivate(QWindow *window)
        : parentWindow(window)
    {
    }

    QWindow *parentWindow;
    QString source;
};

WallpaperWindow::~WallpaperWindow()
{
    s_map.remove(d->parentWindow);
}

QString WallpaperWindow::source()
{
    return d->source;
}

void WallpaperWindow::setSource(const QString &source)
{
    if (d->source == source) {
        return;
    }

    d->source = source;
    Q_EMIT sourceChanged();
}

bool WallpaperWindow::eventFilter(QObject *watched, QEvent *event)
{
    auto window = qobject_cast<QWindow *>(watched);
    if (!window) {
        return false;
    }
    if (event->type() == QEvent::PlatformSurface) {
        if (auto pse = static_cast<QPlatformSurfaceEvent *>(event); pse->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
            initializeShellIntegration();
        }
    }
    return false;
}

WallpaperWindow *WallpaperWindow::get(QWindow *window)
{
    if (!window) {
        return nullptr;
    }

    auto shellWindow = s_map.value(window);
    if (shellWindow) {
        return shellWindow;
    }

    return new WallpaperWindow(window);
}

WallpaperWindow *WallpaperWindow::qmlAttachedProperties(QObject *object)
{
    return get(qobject_cast<QWindow *>(object));
}

QWindow *WallpaperWindow::parentWindow() const
{
    return d->parentWindow;
}

void WallpaperWindow::initializeShellIntegration()
{
    auto waylandWindow = dynamic_cast<QtWaylandClient::QWaylandWindow *>(d->parentWindow->handle());
    if (!waylandWindow) {
        qCWarning(WALLPAPER) << d->parentWindow << "is not a wayland window. Not creating treeland_wallpaper_surface_v1";
        return;
    }

    static QWaylandWallpaperShellIntegration *shellIntegration = nullptr;
    if (!shellIntegration) {
        shellIntegration = new QWaylandWallpaperShellIntegration();
        if (!shellIntegration->initialize(waylandWindow->display())) {
            delete shellIntegration;
            shellIntegration = nullptr;
            qCWarning(WALLPAPER) << "Failed to initialize treeland_wallpaper_produce integration, possibly because compositor does not support the treeland_wallpaper_produce_v1 protocol";
            return;
        }
    }

    waylandWindow->setShellIntegration(shellIntegration);
}

WallpaperWindow::WallpaperWindow(QWindow *window)
    : QObject(window)
    , d(new WallpaperWindowPrivate(window))
{
    s_map.insert(d->parentWindow, this);
    window->installEventFilter(this);

    if (window->isVisible()) {
        qCWarning(WALLPAPER) << d->parentWindow << "call QWindow::close() first and show it again";
    }

    if (window->handle()) {
        initializeShellIntegration();
    }
}
