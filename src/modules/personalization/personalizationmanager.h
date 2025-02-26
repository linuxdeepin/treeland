// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "modules/personalization/impl/appearance_impl.h"
#include "modules/personalization/impl/personalization_manager_impl.h"
#include "modules/personalization/impl/types.h"

#include <wserver.h>
#include <wxdgsurface.h>
#include <WWrapPointer>

#include <DConfig>

#include <QObject>
#include <QQmlEngine>
#include <QQuickItem>

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class SurfaceWrapper;
class PersonalizationV1;

class Personalization : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int32_t backgroundType READ backgroundType NOTIFY backgroundTypeChanged)
    Q_PROPERTY(int32_t cornerRadius READ cornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(Shadow shadow READ shadow NOTIFY shadowChanged)
    Q_PROPERTY(Border border READ border NOTIFY borderChanged)
    Q_PROPERTY(bool noTitlebar READ noTitlebar NOTIFY windowStateChanged)

public:
    enum BackgroundType
    {
        Normal,
        Wallpaper,
        Blur
    };
    Q_ENUM(BackgroundType)

    Personalization(WToplevelSurface *target,
                    PersonalizationV1 *manager,
                    SurfaceWrapper *parent);

    SurfaceWrapper *surfaceWrapper() const;
    Personalization::BackgroundType backgroundType() const;

    int32_t cornerRadius() const
    {
        return m_cornerRadius;
    }

    Shadow shadow() const
    {
        return m_shadow;
    }

    Border border() const
    {
        return m_border;
    }

    bool noTitlebar() const;

Q_SIGNALS:
    void backgroundTypeChanged();
    void cornerRadiusChanged();
    void shadowChanged();
    void borderChanged();
    void windowStateChanged();

private:
    WWrapPointer<WToplevelSurface> m_target;
    PersonalizationV1 *m_manager;
    int32_t m_backgroundType;
    int32_t m_cornerRadius;
    Shadow m_shadow;
    Border m_border;
    personalization_window_context_v1::WindowStates m_states;

    QMetaObject::Connection m_connection;
};

class PersonalizationV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT

    Q_PROPERTY(uid_t userId READ userId WRITE setUserId NOTIFY userIdChanged FINAL)

public:
    explicit PersonalizationV1(QObject *parent = nullptr);
    ~PersonalizationV1();

    void onWindowContextCreated(personalization_window_context_v1 *context);
    void onWallpaperContextCreated(personalization_wallpaper_context_v1 *context);
    void onCursorContextCreated(personalization_cursor_context_v1 *context);
    void onAppearanceContextCreated(personalization_appearance_context_v1 *context);
    void onFontContextCreated(personalization_font_context_v1 *context);

    void onWindowPersonalizationChanged();
    void onWallpaperCommit(personalization_wallpaper_context_v1 *context);
    void onGetWallpapers(personalization_wallpaper_context_v1 *context);

    [[deprecated]] void onCursorCommit(personalization_cursor_context_v1 *context);

    uid_t userId();
    void setUserId(uid_t uid);

    QByteArrayView interfaceName() const override;

    personalization_window_context_v1 *getWindowContext(WSurface *surface);

    QString defaultWallpaper() const;

Q_SIGNALS:
    void userIdChanged(uid_t uid);
    void backgroundChanged(const QString &output, bool isdark);
    void lockscreenChanged();
    void windowContextCreated(personalization_window_context_v1 *context);

public Q_SLOTS:
    QString background(const QString &output, int workspaceId = 1);
    QString lockscreen(const QString &output, int workspaceId = 1);
    bool backgroundIsDark(const QString &output, int workspaceId = 1);
    bool isAnimagedImage(const QString &source);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    void saveImage(personalization_wallpaper_context_v1 *context, const QString &prefix);
    void updateCacheWallpaperPath(uid_t uid);
    QString readWallpaperSettings(const QString &group, const QString &output, int workspaceId = 1);

    uid_t m_userId = 0;
    QString m_cacheDirectory;
    QString m_settingFile;
    QString m_iniMetaData;
    treeland_personalization_manager_v1 *m_manager = nullptr;
    QList<personalization_window_context_v1 *> m_windowContexts;
    std::vector<personalization_appearance_context_v1 *> m_appearanceContexts;
    std::vector<personalization_font_context_v1 *> m_fontContexts;
};
