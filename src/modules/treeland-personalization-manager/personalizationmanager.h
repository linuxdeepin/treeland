// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "impl/personalization_manager_impl.h"

#include <wquickwaylandserver.h>
#include <wxdgsurface.h>

#include <DConfig>

#include <QObject>
#include <QQmlEngine>
#include <QQuickItem>

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class QuickPersonalizationManager;

class QuickPersonalizationManagerAttached : public QObject
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

class QuickPersonalizationManager : public WQuickWaylandServerInterface
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PersonalizationManager)
    QML_ATTACHED(QuickPersonalizationManagerAttached)

    Q_PROPERTY(uid_t userId READ userId WRITE setUserId NOTIFY userIdChanged FINAL)
    Q_PROPERTY(QString cursorTheme READ cursorTheme WRITE setCursorTheme NOTIFY cursorThemeChanged FINAL)
    Q_PROPERTY(QSize cursorSize READ cursorSize WRITE setCursorSize NOTIFY cursorSizeChanged FINAL)

public:
    explicit QuickPersonalizationManager(QObject *parent = nullptr);

    void onWindowContextCreated(personalization_window_context_v1 *context);
    void onWallpaperContextCreated(personalization_wallpaper_context_v1 *context);
    void onCursorContextCreated(personalization_cursor_context_v1 *context);

    void onBackgroundTypeChanged(personalization_window_context_v1 *context);
    void onWallpaperCommit(personalization_wallpaper_context_v1 *context);
    void onGetWallpapers(personalization_wallpaper_context_v1 *context);

    void onCursorCommit(personalization_cursor_context_v1 *context);
    void onGetCursorTheme(personalization_cursor_context_v1 *context);
    void onGetCursorSize(personalization_cursor_context_v1 *context);

    static QuickPersonalizationManagerAttached *qmlAttachedProperties(QObject *target);

    uid_t userId();
    void setUserId(uid_t uid);

    QString cursorTheme();
    void setCursorTheme(const QString &name);

    QSize cursorSize();
    void setCursorSize(const QSize &size);

Q_SIGNALS:
    void backgroundTypeChanged(WSurface *surface, uint32_t type);
    void userIdChanged(uid_t uid);
    void backgroundChanged();
    void lockscreenChanged();
    void cursorThemeChanged(const QString &name);
    void cursorSizeChanged(const QSize &size);

public Q_SLOTS:
    QString background(WOutput *w_output);
    QString lockscreen(WOutput *w_output);

private:
    WServerInterface *create() override;

    QString saveImage(personalization_wallpaper_context_v1 *context, const QString file);
    void updateCacheWallpaperPath(uid_t uid);
    QString readWallpaperSettings(const QString &group, WOutput *w_output);
    void saveWallpaperSettings(const QString &current,
                               personalization_wallpaper_context_v1 *context);

    uid_t m_userId = 0;
    QString m_cacheDirectory;
    QString m_settingFile;
    QString m_iniMetaData;
    QScopedPointer<DTK_CORE_NAMESPACE::DConfig> m_cursorConfig;
    treeland_personalization_manager_v1 *m_manager = nullptr;
};
