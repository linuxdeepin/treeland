// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>
#include <QObject>

struct personalization_window_context_v1;
struct treeland_personalization_manager_v1;
struct personalization_wallpaper_context_v1;

QW_USE_NAMESPACE

QW_BEGIN_NAMESPACE
class QWDisplay;
QW_END_NAMESPACE

class TreeLandPersonalizationManager;
class PersonalizationWindowContext;
class PersonalizationWallpaperContext;
class TreeLandPersonalizationManagerPrivate;
class QW_EXPORT TreeLandPersonalizationManager : public QObject, public QWObject
{
    Q_OBJECT
    QW_DECLARE_PRIVATE(TreeLandPersonalizationManager)
public:
    inline treeland_personalization_manager_v1 *handle() const {
        return QWObject::handle<treeland_personalization_manager_v1>();
    }

    static TreeLandPersonalizationManager *get(treeland_personalization_manager_v1 *handle);
    static TreeLandPersonalizationManager *from(treeland_personalization_manager_v1 *handle);
    static TreeLandPersonalizationManager *create(QWDisplay *display);

    void onSendUserWallpapers(personalization_wallpaper_context_v1 *wallpaper);

Q_SIGNALS:
    void beforeDestroy(TreeLandPersonalizationManager *self);
    void windowContextCreated(PersonalizationWindowContext *context);
    void wallpaperContextCreated(PersonalizationWallpaperContext *context);

private:
    TreeLandPersonalizationManager(treeland_personalization_manager_v1 *handle, bool isOwner);
    ~TreeLandPersonalizationManager() = default;
};

class PersonalizationWindowContextPrivate;
class PersonalizationWindowContext : public QObject, public QWObject
{
    Q_OBJECT
    QW_DECLARE_PRIVATE(PersonalizationWindowContext)
public:
    ~PersonalizationWindowContext() = default;

    inline personalization_window_context_v1 *handle() const {
        return QWObject::handle<personalization_window_context_v1>();
    }

    static PersonalizationWindowContext *get(personalization_window_context_v1 *handle);
    static PersonalizationWindowContext *from(personalization_window_context_v1 *handle);
    static PersonalizationWindowContext *create(personalization_window_context_v1 *handle);

Q_SIGNALS:
    void beforeDestroy(PersonalizationWindowContext *self);
    void backgroundTypeChanged(PersonalizationWindowContext *handle);

private:
    PersonalizationWindowContext(personalization_window_context_v1 *handle, bool isOwner);
};


class PersonalizationWallpaperContextPrivate;
class PersonalizationWallpaperContext : public QObject, public QWObject
{
    Q_OBJECT
    QW_DECLARE_PRIVATE(PersonalizationWallpaperContext)
public:
    ~PersonalizationWallpaperContext() = default;

    inline personalization_wallpaper_context_v1 *handle() const {
        return QWObject::handle<personalization_wallpaper_context_v1>();
    }

    static PersonalizationWallpaperContext *get(personalization_wallpaper_context_v1 *handle);
    static PersonalizationWallpaperContext *from(personalization_wallpaper_context_v1 *handle);
    static PersonalizationWallpaperContext *create(personalization_wallpaper_context_v1 *handle);

Q_SIGNALS:
    void beforeDestroy(PersonalizationWallpaperContext *self);
    void setUserWallpaper(personalization_wallpaper_context_v1 *handle);
    void getUserWallpaper(personalization_wallpaper_context_v1 *handle);

private:
    PersonalizationWallpaperContext(personalization_wallpaper_context_v1 *handle, bool isOwner);
};
