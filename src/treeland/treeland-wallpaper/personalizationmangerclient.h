// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QQmlEngine>
#include <QGuiApplication>
#include <QtWaylandClient/QWaylandClientExtension>

#include "qwayland-treeland-personalization-manager-v1.h"

class PersonalizationWindow;
class PersonalizationWallpaper;

class PersonalizationManager : public QWaylandClientExtensionTemplate<PersonalizationManager>, public QtWayland::treeland_personalization_manager_v1
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit PersonalizationManager();
    ~PersonalizationManager();

    void onActiveChanged();

signals:
    void wallpaperChanged(const QString &path);

public slots:
    void setWallpaper(const QString &path);

private:
    PersonalizationWallpaper* m_wallpaperContext = nullptr;
};

class PersonalizationWindow : public QWaylandClientExtensionTemplate<PersonalizationWindow>, public QtWayland::personalization_window_context_v1
{
    Q_OBJECT
public:
    explicit PersonalizationWindow(struct ::personalization_window_context_v1 *object);
};

class PersonalizationWallpaper : public QWaylandClientExtensionTemplate<PersonalizationWallpaper>, public QtWayland::personalization_wallpaper_context_v1
{
    Q_OBJECT
public:
    explicit PersonalizationWallpaper(struct ::personalization_wallpaper_context_v1 *object);

signals:
    void wallpaperChanged(const QString &path);

protected:
    void personalization_wallpaper_context_v1_wallpapers(wl_array *paths) override;
};
