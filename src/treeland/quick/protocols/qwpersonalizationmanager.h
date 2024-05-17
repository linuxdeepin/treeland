// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "helper.h"
#include "personalization-server-protocol.h"
#include "personalization_manager_impl.h"

#include <wquickwaylandserver.h>
#include <wxdgsurface.h>

#include <QObject>
#include <QQmlEngine>
#include <qtmetamacros.h>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WXdgSurface;
class WSurface;
WAYLIB_SERVER_END_NAMESPACE

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class QDir;
class QSettings;
class QuickPersonalizationManager;

class WAYLIB_SERVER_EXPORT QuickPersonalizationManagerAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(BackgroundType backgroundType READ backgroundType NOTIFY backgroundTypeChanged FINAL)
    Q_PROPERTY(QQuickItem* backgroundImage READ backgroundImage FINAL)
    QML_ANONYMOUS

public:
    enum BackgroundType { Normal, Wallpaper, Blend };
    Q_ENUM(BackgroundType)

    QuickPersonalizationManagerAttached(WSurface *target, QuickPersonalizationManager *manager);
    QuickPersonalizationManagerAttached(QQuickItem *target, QuickPersonalizationManager *manager);

    BackgroundType backgroundType() const { return m_backgroundType; };

    QQuickItem *backgroundImage() const;

Q_SIGNALS:
    void backgroundTypeChanged();

private:
    QObject *m_target;
    QuickPersonalizationManager *m_manager;
    BackgroundType m_backgroundType = Normal;
};

class QuickPersonalizationManagerPrivate;
class PersonalizationWindowContext;
class PersonalizationWallpaperContext;

class QuickPersonalizationManager : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PersonalizationManager)
    W_DECLARE_PRIVATE(QuickPersonalizationManager)
    QML_ATTACHED(QuickPersonalizationManagerAttached)

    Q_PROPERTY(QString currentWallpaper READ currentWallpaper WRITE setCurrentWallpaper NOTIFY currentWallpaperChanged FINAL)
    Q_PROPERTY(uid_t currentUserId READ currentUserId WRITE setCurrentUserId NOTIFY currentUserIdChanged FINAL)

public:
    explicit QuickPersonalizationManager(QObject *parent = nullptr);

    void onWindowContextCreated(PersonalizationWindowContext *context);
    void onWallpaperContextCreated(PersonalizationWallpaperContext *context);
    void onBackgroundTypeChanged(PersonalizationWindowContext *context);
    void onCommit(personalization_wallpaper_context_v1 *context);
    void onGetWallpapers(personalization_wallpaper_context_v1 *context);
    static QuickPersonalizationManagerAttached *qmlAttachedProperties(QObject *target);

    QString currentWallpaper();
    void setCurrentWallpaper(const QString &path);

    uid_t currentUserId();
    void setCurrentUserId(uid_t uid);

Q_SIGNALS:
    void backgroundTypeChanged(WSurface *surface, uint32_t type);
    void sendUserwallpapers(personalization_wallpaper_context_v1 *wallpaper);
    void currentWallpaperChanged(const QString &wallpaper);
    void currentUserIdChanged(uid_t uid);

private:
    void create() override;
};
