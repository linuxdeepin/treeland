// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalizationmangerclient.h"

#include <QDebug>
#include <QObject>
#include <QDBusInterface>
#include <QtWaylandClient/QWaylandClientExtension>

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

PersonalizationManager::PersonalizationManager()
    : QWaylandClientExtensionTemplate<PersonalizationManager>(1)
{
    connect(this, &PersonalizationManager::activeChanged, this, &PersonalizationManager::onActiveChanged);
}

PersonalizationManager::~PersonalizationManager()
{
    if (m_wallpaperContext) {
        m_wallpaperContext->deleteLater();
    }
}

void PersonalizationManager::onActiveChanged()
{
    if (!isActive())
        return;

    if (!m_wallpaperContext) {
        m_wallpaperContext = new PersonalizationWallpaper(get_wallpaper_context());
        connect(m_wallpaperContext, &PersonalizationWallpaper::wallpaperChanged, this, &PersonalizationManager::wallpaperChanged);
    }

    m_wallpaperContext->get_wallpapers();
}

void PersonalizationManager::setWallpaper(const QString &path)
{
    if (!m_wallpaperContext)
        return;

    m_wallpaperContext->set_wallpaper(QUrl(path).toLocalFile());

    m_wallpaperContext->get_wallpapers();
}

PersonalizationWindow::PersonalizationWindow(struct ::personalization_window_context_v1 *object)
    : QWaylandClientExtensionTemplate<PersonalizationWindow>(1)
    , QtWayland::personalization_window_context_v1(object)
{

}

PersonalizationWallpaper::PersonalizationWallpaper(struct ::personalization_wallpaper_context_v1 *object)
    : QWaylandClientExtensionTemplate<PersonalizationWallpaper>(1)
    , QtWayland::personalization_wallpaper_context_v1(object)
{

}

void PersonalizationWallpaper::personalization_wallpaper_context_v1_wallpapers(wl_array *paths)
{
    char *current = (char *)paths->data;
    QString wallpaper;
    while ((size_t)(current - (char *)paths->data) < paths->size) {
        wallpaper = "file://" + QString::fromUtf8(current);

        current += strlen(current) + 1;
    }

    Q_EMIT wallpaperChanged(wallpaper);
}
