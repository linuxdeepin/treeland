// Copyright (C) 2024 rewine <luhongxu@deepin.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalization_manager.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

PersonalizationManager::PersonalizationManager()
    : QWaylandClientExtensionTemplate<PersonalizationManager>(1)
{
}

PersonalizationWindow::PersonalizationWindow(
    struct ::treeland_personalization_window_context_v1 *object)
    : QWaylandClientExtensionTemplate<PersonalizationWindow>(1)
    , QtWayland::treeland_personalization_window_context_v1(object)
{
}

PersonalizationWallpaper::PersonalizationWallpaper(
    struct ::treeland_personalization_wallpaper_context_v1 *object)
    : QWaylandClientExtensionTemplate<PersonalizationWallpaper>(1)
    , QtWayland::treeland_personalization_wallpaper_context_v1(object)
{
}

void PersonalizationWallpaper::treeland_personalization_wallpaper_context_v1_metadata(
    const QString &metadata)
{
    qDebug() << "=========================================== metadata" << metadata;
}

PersonalizationCursor::PersonalizationCursor(
    struct ::treeland_personalization_cursor_context_v1 *object)
    : QWaylandClientExtensionTemplate<PersonalizationCursor>(1)
    , QtWayland::treeland_personalization_cursor_context_v1(object)
{
}

void PersonalizationCursor::personalization_cursor_context_v1_verfity(int32_t success)
{
    qDebug() << "=========================================== verfity" << success;
}

void PersonalizationCursor::personalization_cursor_context_v1_theme(const QString &name)
{
    qDebug() << "=========================================== theme" << name;
}

void PersonalizationCursor::personalization_cursor_context_v1_size(uint32_t size)
{
    qDebug() << "=========================================== size" << size;
}

QT_END_NAMESPACE
