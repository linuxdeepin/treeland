// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "impl/appearance_impl.h"
#include "impl/personalization_manager_impl.h"

#include <wserver.h>
#include <wxdgsurface.h>

#include <DConfig>

#include <QObject>
#include <QQmlEngine>
#include <QQuickItem>

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class PersonalizationV1;
class QuickPersonalizationManagerAttached;

class Personalization : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Personalization)
    QML_UNCREATABLE("Only use for the enums.")
    QML_ATTACHED(QuickPersonalizationManagerAttached)
public:
    using QObject::QObject;

    enum BackgroundType { Normal, Wallpaper, Blend };
    Q_ENUM(BackgroundType)

    static QuickPersonalizationManagerAttached *qmlAttachedProperties(QObject *target);
};

class QuickPersonalizationManagerAttached : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int32_t backgroundType READ backgroundType NOTIFY backgroundTypeChanged)
    Q_PROPERTY(int32_t cornerRadius READ cornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(Shadow shadow READ shadow NOTIFY shadowChanged)
    Q_PROPERTY(Border border READ border NOTIFY borderChanged)
    Q_PROPERTY(bool noTitlebar READ noTitlebar NOTIFY windowStateChanged)

public:
    QuickPersonalizationManagerAttached(WToplevelSurface *target, PersonalizationV1 *manager);

    Personalization::BackgroundType backgroundType() const;

    int32_t cornerRadius() const { return m_cornerRadius; }

    Shadow shadow() const { return m_shadow; }

    Border border() const { return m_border; }

    bool noTitlebar() const
    {
        return m_states.testFlag(personalization_window_context_v1::noTitlebar);
    }

Q_SIGNALS:
    void backgroundTypeChanged();
    void cornerRadiusChanged();
    void shadowChanged();
    void borderChanged();
    void windowStateChanged();

private:
    WToplevelSurface *m_target;
    PersonalizationV1 *m_manager;
    int32_t m_backgroundType;
    int32_t m_cornerRadius;
    Shadow m_shadow;
    Border m_border;
    personalization_window_context_v1::WindowStates m_states;
};

class PersonalizationV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT

    Q_PROPERTY(uid_t userId READ userId WRITE setUserId NOTIFY userIdChanged FINAL)
    Q_PROPERTY(QString cursorTheme READ cursorTheme WRITE setCursorTheme NOTIFY cursorThemeChanged FINAL)
    Q_PROPERTY(QSize cursorSize READ cursorSize WRITE setCursorSize NOTIFY cursorSizeChanged FINAL)

public:
    explicit PersonalizationV1(QObject *parent = nullptr);

    void onWindowContextCreated(personalization_window_context_v1 *context);
    void onWallpaperContextCreated(personalization_wallpaper_context_v1 *context);
    void onCursorContextCreated(personalization_cursor_context_v1 *context);
    void onAppearanceContextCreated(personalization_appearance_context_v1 *context);

    void onWindowPersonalizationChanged();
    void onWallpaperCommit(personalization_wallpaper_context_v1 *context);
    void onGetWallpapers(personalization_wallpaper_context_v1 *context);

    void onCursorCommit(personalization_cursor_context_v1 *context);
    void onGetCursorTheme(personalization_cursor_context_v1 *context);
    void onGetCursorSize(personalization_cursor_context_v1 *context);

    uid_t userId();
    void setUserId(uid_t uid);

    QString cursorTheme();
    void setCursorTheme(const QString &name);

    QSize cursorSize();
    void setCursorSize(const QSize &size);

    int32_t windowRadius() const;

    QString fontName() const;

    QString monoFontName() const;

    QString iconTheme() const;

    QByteArrayView interfaceName() const override;

    personalization_window_context_v1 *getWindowContext(WSurface *surface);

Q_SIGNALS:
    void userIdChanged(uid_t uid);
    void backgroundChanged(const QString &output, bool isdark);
    void lockscreenChanged();
    void cursorThemeChanged(const QString &name);
    void cursorSizeChanged(const QSize &size);
    void windowContextCreated(personalization_window_context_v1 *context);

public Q_SLOTS:
    QString background(const QString &output);
    QString lockscreen(const QString &output);
    bool backgroundIsDark(const QString &output);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    void writeContext(personalization_wallpaper_context_v1 *context,
                      const QByteArray &data,
                      const QString &dest);
    void saveImage(personalization_wallpaper_context_v1 *context, const QString &prefix);
    void updateCacheWallpaperPath(uid_t uid);
    QString readWallpaperSettings(const QString &group, const QString &output);
    void saveWallpaperSettings(const QString &current,
                               personalization_wallpaper_context_v1 *context);

    uid_t m_userId = 0;
    QString m_cacheDirectory;
    QString m_settingFile;
    QString m_iniMetaData;
    QScopedPointer<DTK_CORE_NAMESPACE::DConfig> m_dconfig;
    treeland_personalization_manager_v1 *m_manager = nullptr;
    QList<personalization_window_context_v1 *> m_windowContexts;
};
