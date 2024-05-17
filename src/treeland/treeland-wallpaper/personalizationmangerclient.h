// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-treeland-personalization-manager-v1.h"
#include "wallpapercardmodel.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QtWaylandClient/QWaylandClientExtension>

class PersonalizationWindow;
class PersonalizationWallpaper;

class PersonalizationManager : public QWaylandClientExtensionTemplate<PersonalizationManager>,
                               public QtWayland::treeland_personalization_manager_v1
{
    Q_OBJECT
    Q_PROPERTY(QString currentGroup READ currentGroup WRITE setCurrentGroup NOTIFY currentGroupChanged FINAL)
    Q_PROPERTY(QString cacheDirectory READ cacheDirectory CONSTANT)
    QML_ELEMENT
public:
    struct WallpaperMetaData
    {
        QString group;
        QString imagePath; // wallpaper path
        QString output;    // Output which wallpaper belongs
        int currentIndex;
    };

    explicit PersonalizationManager();
    ~PersonalizationManager();

    void onActiveChanged();
    QString cacheDirectory();

    QString currentGroup();
    void setCurrentGroup(const QString &group);

signals:
    void wallpaperChanged(const QString &path);
    void currentGroupChanged(const QString &path);

public slots:
    void addWallpaper(const QString &path);
    void setWallpaper(const QString &path, const QString &group, int index);
    void removeWallpaper(const QString &path, const QString &group, int index);

    WallpaperCardModel *wallpaperModel(const QString &group, const QString &dir);

private:
    QString converToJson(WallpaperMetaData *data);
    void onWallpaperChanged(const QString &meta);

private:
    PersonalizationWallpaper *m_wallpaperContext = nullptr;
    WallpaperMetaData *m_metaData = nullptr;
    QMap<QString, WallpaperCardModel *> m_modes;
    QString m_cacheDirectory;
};

class PersonalizationWindow : public QWaylandClientExtensionTemplate<PersonalizationWindow>,
                              public QtWayland::personalization_window_context_v1
{
    Q_OBJECT
public:
    explicit PersonalizationWindow(struct ::personalization_window_context_v1 *object);
};

class PersonalizationWallpaper : public QWaylandClientExtensionTemplate<PersonalizationWallpaper>,
                                 public QtWayland::personalization_wallpaper_context_v1
{
    Q_OBJECT
public:
    explicit PersonalizationWallpaper(struct ::personalization_wallpaper_context_v1 *object);

signals:
    void wallpaperChanged(const QString &meta);

protected:
    void personalization_wallpaper_context_v1_wallpapers(const QString &metadata) override;
};
