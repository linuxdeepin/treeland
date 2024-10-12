// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalizationmanager.h"

#include "impl/appearance_impl.h"
#include "impl/personalization_manager_impl.h"

#include <wlayersurface.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwlayershellv1.h>
#include <qwsignalconnector.h>
#include <qwxdgshell.h>

#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

#include <sys/socket.h>
#include <unistd.h>

DCORE_USE_NAMESPACE

static PersonalizationV1 *PERSONALIZATION_MANAGER = nullptr;

#define DEFAULT_WALLPAPER ":/desktop.webp"
#define DEFAULT_WALLPAPER_ISDARK false

QuickPersonalizationManagerAttached *Personalization::qmlAttachedProperties(QObject *target)
{
    if (auto *surface = qobject_cast<WToplevelSurface *>(target)) {
        return new QuickPersonalizationManagerAttached(surface, PERSONALIZATION_MANAGER);
    }

    return nullptr;
}

void PersonalizationV1::updateCacheWallpaperPath(uid_t uid)
{
    QString cache_location = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    m_cacheDirectory = cache_location + QString("/wallpaper/%1/").arg(uid);
    m_settingFile = m_cacheDirectory + "wallpaper.ini";

    QSettings settings(m_settingFile, QSettings::IniFormat);
    m_iniMetaData = settings.value("metadata").toString();
}

QString PersonalizationV1::readWallpaperSettings(const QString &group, const QString &output)
{
    if (m_settingFile.isEmpty() || output.isEmpty())
        return DEFAULT_WALLPAPER;

    QSettings settings(m_settingFile, QSettings::IniFormat);
    QString value = settings.value(group + "/" + output, DEFAULT_WALLPAPER).toString();
    return value == DEFAULT_WALLPAPER ? value : QString("file://%1").arg(value);
}

void PersonalizationV1::saveWallpaperSettings(const QString &current,
                                              personalization_wallpaper_context_v1 *context)
{
    if (m_settingFile.isEmpty() || context == nullptr)
        return;

    QSettings settings(m_settingFile, QSettings::IniFormat);

    if (context->options & TREELAND_PERSONALIZATION_WALLPAPER_CONTEXT_V1_OPTIONS_BACKGROUND) {
        settings.setValue(QString("background/%1").arg(context->output_name), current);
        settings.setValue(QString("background/%1/isdark").arg(context->output_name),
                          context->isdark);
    }

    if (context->options & TREELAND_PERSONALIZATION_WALLPAPER_CONTEXT_V1_OPTIONS_LOCKSCREEN) {
        settings.setValue(QString("lockscreen/%1").arg(context->output_name), current);
        settings.setValue(QString("background/%1/isdark").arg(context->output_name),
                          context->isdark);
    }

    settings.setValue("metadata", context->meta_data);
    m_iniMetaData = context->meta_data;
}

PersonalizationV1::PersonalizationV1(QObject *parent)
    : QObject(parent)
    , m_dconfig(DConfig::create("org.deepin.treeland", "org.deepin.treeland", QString()))
{
    if (PERSONALIZATION_MANAGER) {
        qFatal("There are multiple instances of QuickPersonalizationManager");
    }

    Q_INIT_RESOURCE(default_background);

    PERSONALIZATION_MANAGER = this;

    // When not use ddm, set uid by self
    if (qgetenv("XDG_SESSION_DESKTOP") == "treeland-user") {
        setUserId(getgid());
    }
}

PersonalizationV1::~PersonalizationV1()
{
    PERSONALIZATION_MANAGER = nullptr;

    Q_CLEANUP_RESOURCE(default_background);
}

void PersonalizationV1::onWindowContextCreated(personalization_window_context_v1 *context)
{
    connect(context, &personalization_window_context_v1::before_destroy, this, [this, context] {
        m_windowContexts.removeAll(context);
    });

    m_windowContexts.append(context);

    Q_EMIT windowContextCreated(context);
}

void PersonalizationV1::onWallpaperContextCreated(personalization_wallpaper_context_v1 *context)
{
    connect(context,
            &personalization_wallpaper_context_v1::commit,
            this,
            &PersonalizationV1::onWallpaperCommit);
    connect(context,
            &personalization_wallpaper_context_v1::getWallpapers,
            this,
            &PersonalizationV1::onGetWallpapers);
}

void PersonalizationV1::onCursorContextCreated(personalization_cursor_context_v1 *context)
{
    connect(context,
            &personalization_cursor_context_v1::commit,
            this,
            &PersonalizationV1::onCursorCommit);
    connect(context,
            &personalization_cursor_context_v1::get_theme,
            this,
            &PersonalizationV1::onGetCursorTheme);
    connect(context,
            &personalization_cursor_context_v1::get_size,
            this,
            &PersonalizationV1::onGetCursorSize);
}

void PersonalizationV1::onAppearanceContextCreated(personalization_appearance_context_v1 *context)
{
    using Appearance = personalization_appearance_context_v1;

    connect(context, &Appearance::roundCornerRadiusChanged, this, [this, context] {
        m_dconfig->setValue("roundCornerRadius", context->cursorTheme());
    });
    connect(context, &Appearance::fontChanged, this, [this, context] {
        m_dconfig->setValue("font", context->cursorTheme());
    });
    connect(context, &Appearance::monoFontChanged, this, [this, context] {
        m_dconfig->setValue("monoFont", context->cursorTheme());
    });
    connect(context, &Appearance::cursorThemeChanged, this, [this, context] {
        m_dconfig->setValue("cursorThemeName", context->cursorTheme());
    });
    connect(context, &Appearance::iconThemeChanged, this, [this, context] {
        m_dconfig->setValue("iconThemeName", context->cursorTheme());
    });

    context->blockSignals(true);

    context->setRoundCornerRadius(windowRadius());
    context->setFont(fontName().toUtf8());
    context->setMonospaceFont(monoFontName().toUtf8());
    context->setCursorTheme(cursorTheme().toUtf8());
    context->setIconTheme(iconTheme().toUtf8());

    context->blockSignals(false);
}

void PersonalizationV1::writeContext(personalization_wallpaper_context_v1 *context,
                                     const QByteArray &data,
                                     const QString &dest)
{
    QFile dest_file(dest);
    if (dest_file.open(QIODevice::WriteOnly)) {
        dest_file.write(data);
        dest_file.close();

        saveWallpaperSettings(dest, context);
        Q_EMIT backgroundChanged(context->output_name, context->isdark);
    }
}

void PersonalizationV1::saveImage(personalization_wallpaper_context_v1 *context,
                                  const QString &prefix)
{
    if (!context || context->fd == -1)
        return;

    QFile src_file;
    if (!src_file.open(context->fd, QIODevice::ReadOnly))
        return;

    QByteArray data = src_file.readAll();
    src_file.close();

    QDir dir(m_cacheDirectory);
    if (!dir.exists()) {
        dir.mkpath(m_cacheDirectory);
    }

    QString dest = m_cacheDirectory + prefix + "_" + context->output_name;
    if (context->output_name.isEmpty()) {
        for (QScreen *screen : QGuiApplication::screens()) {
            context->output_name = screen->name();
            dest = m_cacheDirectory + prefix + "_" + screen->name();
            writeContext(context, data, dest);
        }
    } else {
        writeContext(context, data, dest);
    }
}

void PersonalizationV1::onWallpaperCommit(personalization_wallpaper_context_v1 *context)
{
    if (context->options & TREELAND_PERSONALIZATION_WALLPAPER_CONTEXT_V1_OPTIONS_BACKGROUND) {
        saveImage(context, "background");
    }

    if (context->options & TREELAND_PERSONALIZATION_WALLPAPER_CONTEXT_V1_OPTIONS_LOCKSCREEN) {
        saveImage(context, "lockscreen");
    }
}

void PersonalizationV1::onCursorCommit(personalization_cursor_context_v1 *context)
{
    if (m_dconfig == nullptr || !m_dconfig->isValid()) {
        context->verfity(false);
        return;
    }

    if (context->size > 0)
        setCursorSize(QSize(context->size, context->size));

    if (!context->theme.isEmpty())
        setCursorTheme(context->theme);

    context->verfity(true);
}

void PersonalizationV1::onGetCursorTheme(personalization_cursor_context_v1 *context)
{
    if (m_dconfig == nullptr || !m_dconfig->isValid())
        return;

    context->set_theme(cursorTheme());
}

void PersonalizationV1::onGetCursorSize(personalization_cursor_context_v1 *context)
{
    if (m_dconfig == nullptr)
        return;

    context->set_size(cursorSize().width());
}

void PersonalizationV1::onGetWallpapers(personalization_wallpaper_context_v1 *context)
{
    QDir dir(m_cacheDirectory);
    if (!dir.exists())
        return;

    context->set_meta_data(m_iniMetaData);
}

uid_t PersonalizationV1::userId()
{
    return m_userId;
}

void PersonalizationV1::setUserId(uid_t uid)
{
    m_userId = uid;
    updateCacheWallpaperPath(uid);
    Q_EMIT userIdChanged(uid);
}

QString PersonalizationV1::cursorTheme()
{
    QString value = m_dconfig->value("cursorThemeName", "default").toString();
    return value;
}

void PersonalizationV1::setCursorTheme(const QString &name)
{
    m_dconfig->setValue("cursorThemeName", name);
    Q_EMIT cursorThemeChanged(name);
}

QSize PersonalizationV1::cursorSize()
{
    int size = m_dconfig->value("cursorSize", 24).toInt();
    return QSize(size, size);
}

void PersonalizationV1::setCursorSize(const QSize &size)
{
    m_dconfig->setValue("cursorSize", size.width());
    Q_EMIT cursorSizeChanged(size);
}

int32_t PersonalizationV1::windowRadius() const
{
    return m_dconfig->value("roundCornerRadius", 18).toInt();
}

QString PersonalizationV1::fontName() const
{
    return m_dconfig->value("font", 18).toString();
}

QString PersonalizationV1::monoFontName() const
{
    return m_dconfig->value("monoFont", 18).toString();
}

QString PersonalizationV1::iconTheme() const
{
    return m_dconfig->value("iconThemeName", 18).toString();
}

QString PersonalizationV1::background(const QString &output)
{
    return readWallpaperSettings("background", output);
}

QString PersonalizationV1::lockscreen(const QString &output)
{
    return readWallpaperSettings("lockscreen", output);
}

bool PersonalizationV1::backgroundIsDark(const QString &output)
{
    if (m_settingFile.isEmpty())
        return DEFAULT_WALLPAPER_ISDARK;

    QSettings settings(m_settingFile, QSettings::IniFormat);
    return settings.value(QString("background/%1/isdark").arg(output), DEFAULT_WALLPAPER_ISDARK)
        .toBool();
}

bool PersonalizationV1::isAnimagedImage(const QString &source)
{
    QImageReader reader(source);
    return reader.imageCount() > 1;
}

QuickPersonalizationManagerAttached::QuickPersonalizationManagerAttached(WToplevelSurface *target,
                                                                         PersonalizationV1 *manager)
    : QObject(manager)
    , m_target(target)
    , m_manager(manager)
{
    auto update = [this](personalization_window_context_v1 *context) {
        if (!context) {
            return;
        }
        connect(context,
                &personalization_window_context_v1::backgroundTypeChanged,
                this,
                [this, context] {
                    m_backgroundType = context->background_type;
                    Q_EMIT backgroundTypeChanged();
                });
        connect(context,
                &personalization_window_context_v1::cornerRadiusChanged,
                this,
                [this, context] {
                    m_cornerRadius = context->corner_radius;
                    Q_EMIT cornerRadiusChanged();
                });

        connect(context, &personalization_window_context_v1::shadowChanged, this, [this, context] {
            m_shadow = context->shadow;
            Q_EMIT shadowChanged();
        });

        connect(context, &personalization_window_context_v1::borderChanged, this, [this, context] {
            m_border = context->border;
            Q_EMIT borderChanged();
        });

        connect(context,
                &personalization_window_context_v1::windowStateChanged,
                this,
                [this, context] {
                    m_states = context->states;
                    Q_EMIT windowStateChanged();
                });
    };

    connect(m_manager, &PersonalizationV1::windowContextCreated, this, update);

    if (auto *context = m_manager->getWindowContext(target->surface())) {
        update(context);
    }
}

Personalization::BackgroundType QuickPersonalizationManagerAttached::backgroundType() const
{
    if (auto *target = qobject_cast<WLayerSurface *>(m_target)) {
        auto scope = QString(target->handle()->handle()->scope);
        QStringList forceList{ "dde-shell/dock", "dde-shell/launchpad" };
        if (forceList.contains(scope)) {
            return Personalization::Blend;
        }
    }
    return static_cast<Personalization::BackgroundType>(m_backgroundType);
}

void PersonalizationV1::create(WServer *server)
{
    m_manager = treeland_personalization_manager_v1::create(server->handle());
    connect(m_manager,
            &treeland_personalization_manager_v1::windowContextCreated,
            this,
            &PersonalizationV1::onWindowContextCreated);
    connect(m_manager,
            &treeland_personalization_manager_v1::wallpaperContextCreated,
            this,
            &PersonalizationV1::onWallpaperContextCreated);
    connect(m_manager,
            &treeland_personalization_manager_v1::cursorContextCreated,
            this,
            &PersonalizationV1::onCursorContextCreated);
    connect(m_manager,
            &treeland_personalization_manager_v1::appearanceContextCreated,
            this,
            &PersonalizationV1::onAppearanceContextCreated);
}

void PersonalizationV1::destroy(WServer *server) { }

wl_global *PersonalizationV1::global() const
{
    return m_manager->global;
}

QByteArrayView PersonalizationV1::interfaceName() const
{
    return treeland_personalization_manager_v1_interface.name;
}

personalization_window_context_v1 *PersonalizationV1::getWindowContext(WSurface *surface)
{
    for (auto *context : m_windowContexts) {
        if (context->surface == surface->handle()->handle()) {
            return context;
        }
    }

    return nullptr;
}
