// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalizationmanager.h"
#include "surfacewrapper.h"

#include "config/treelandconfig.h"
#include "modules/personalization/impl/appearance_impl.h"
#include "modules/personalization/impl/font_impl.h"
#include "modules/personalization/impl/personalization_manager_impl.h"

#include <wlayersurface.h>
#include <wxdgpopupsurface.h>
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

#define DEFAULT_WALLPAPER "qrc:/desktop.webp"
#define DEFAULT_WALLPAPER_ISDARK false

static QString defaultBackground()
{
    static QString defaultBg = [] {
        const QString configDefaultBg = TreelandConfig::ref().defaultBackground();
        return QFile::exists(configDefaultBg) ? configDefaultBg : DEFAULT_WALLPAPER;
    }();
    return defaultBg;
}

void PersonalizationV1::updateCacheWallpaperPath(uid_t uid)
{
    QString cache_location = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    m_cacheDirectory = cache_location + QString("/wallpaper/%1/").arg(uid);
    m_settingFile = m_cacheDirectory + "wallpaper.ini";

    QSettings settings(m_settingFile, QSettings::IniFormat);
    m_iniMetaData = settings.value("metadata").toString();
}

QString PersonalizationV1::readWallpaperSettings(const QString &group,
                                                 const QString &output,
                                                 int workspaceId)
{
    if (m_settingFile.isEmpty() || output.isEmpty() || workspaceId < 1)
        return defaultBackground();

    QSettings settings(m_settingFile, QSettings::IniFormat);
    settings.beginGroup(QString("%1.%2.%3").arg(group).arg(output).arg(workspaceId));
    return settings.value("path", defaultBackground()).toString();
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
            context,
            &personalization_cursor_context_v1::sendTheme);
    connect(context,
            &personalization_cursor_context_v1::get_size,
            context,
            &personalization_cursor_context_v1::sendSize);

    context->blockSignals(true);
    context->setTheme(TreelandConfig::ref().cursorThemeName());
    context->setSize(TreelandConfig::ref().cursorSize());
    context->blockSignals(false);
}

void PersonalizationV1::onAppearanceContextCreated(personalization_appearance_context_v1 *context)
{
    using Appearance = personalization_appearance_context_v1;

    m_appearanceContexts.push_back(context);

    connect(context, &Appearance::roundCornerRadiusChanged, this, [this, context](int32_t radius) {
        TreelandConfig::ref().setWindowRadius(radius);
        for (auto *context : m_appearanceContexts) {
            context->sendRoundCornerRadius(radius);
        }
    });
    connect(context, &Appearance::iconThemeChanged, this, [this, context](const QString &theme) {
        TreelandConfig::ref().setIconThemeName(theme);
        for (auto *context : m_appearanceContexts) {
            context->sendIconTheme(theme.toUtf8());
        }
    });
    connect(context, &Appearance::activeColorChanged, this, [this, context](const QString &color) {
        TreelandConfig::ref().setActiveColor(color);
        for (auto *context : m_appearanceContexts) {
            context->sendActiveColor(color.toUtf8());
        }
    });
    connect(context, &Appearance::windowOpacityChanged, this, [this, context](uint32_t opacity) {
        TreelandConfig::ref().setWindowOpacity(opacity);
        for (auto *context : m_appearanceContexts) {
            context->sendWindowOpacity(opacity);
        }
    });
    connect(context, &Appearance::windowThemeTypeChanged, this, [this, context](int32_t type) {
        TreelandConfig::ref().setWindowThemeType(type);
        for (auto *context : m_appearanceContexts) {
            context->sendWindowThemeType(type);
        }
    });
    connect(context, &Appearance::titlebarHeightChanged, this, [this, context](uint32_t height) {
        TreelandConfig::ref().setWindowTitlebarHeight(height);
        for (auto *context : m_appearanceContexts) {
            context->sendWindowTitlebarHeight(height);
        }
    });

    connect(context, &Appearance::requestRoundCornerRadius, context, [this, context] {
        context->setRoundCornerRadius(windowRadius());
    });

    connect(context, &Appearance::requestIconTheme, context, [this, context] {
        context->setIconTheme(iconTheme().toUtf8());
    });

    connect(context, &Appearance::requestActiveColor, context, [this, context] {
        context->setActiveColor(TreelandConfig::ref().activeColor().toUtf8());
    });

    connect(context, &Appearance::requestWindowOpacity, context, [this, context] {
        context->setWindowOpacity(TreelandConfig::ref().windowOpacity());
    });

    connect(context, &Appearance::requestWindowThemeType, context, [this, context] {
        context->setWindowThemeType(TreelandConfig::ref().windowThemeType());
    });

    connect(context, &Appearance::requestWindowTitlebarHeight, context, [this, context] {
        context->setWindowTitlebarHeight(TreelandConfig::ref().windowTitlebarHeight());
    });

    connect(context, &Appearance::beforeDestroy, this, [this, context] {
        for (auto it = m_appearanceContexts.begin(); it != m_appearanceContexts.end(); ++it) {
            if (*it == context) {
                m_appearanceContexts.erase(it);
                break;
            }
        }
    });

    context->blockSignals(true);

    context->setRoundCornerRadius(TreelandConfig::ref().windowRadius());
    context->setIconTheme(TreelandConfig::ref().iconThemeName().toUtf8());
    context->setActiveColor(TreelandConfig::ref().activeColor().toUtf8());
    context->setWindowOpacity(TreelandConfig::ref().windowOpacity());
    context->setWindowThemeType(TreelandConfig::ref().windowThemeType());
    context->setWindowTitlebarHeight(TreelandConfig::ref().windowTitlebarHeight());

    context->blockSignals(false);
}

void PersonalizationV1::onFontContextCreated(personalization_font_context_v1 *context)
{
    using Font = personalization_font_context_v1;

    connect(&TreelandConfig::ref(), &TreelandConfig::fontNameChanged, context, [context] {
        context->sendFont(TreelandConfig::ref().fontName());
    });
    connect(&TreelandConfig::ref(), &TreelandConfig::monoFontNameChanged, context, [context] {
        context->sendMonospaceFont(TreelandConfig::ref().monoFontName());
    });
    connect(&TreelandConfig::ref(), &TreelandConfig::fontSizeChanged, context, [context] {
        context->sendFontSize(TreelandConfig::ref().fontSize());
    });

    connect(context, &Font::requestFont, context, [context] {
        context->sendFont(TreelandConfig::ref().fontName());
    });
    connect(context, &Font::requestMonoFont, context, [context] {
        context->sendMonospaceFont(TreelandConfig::ref().monoFontName());
    });
    connect(context, &Font::requestFontSize, context, [context] {
        context->sendFontSize(TreelandConfig::ref().fontSize());
    });

    connect(context, &Font::fontChanged, &TreelandConfig::ref(), &TreelandConfig::setFontName);
    connect(context,
            &Font::monoFontChanged,
            &TreelandConfig::ref(),
            &TreelandConfig::setMonoFontName);
    connect(context, &Font::fontSizeChanged, &TreelandConfig::ref(), &TreelandConfig::setFontSize);

    connect(context, &Font::beforeDestroy, this, [this, context] {
        for (auto it = m_fontContexts.begin(); it != m_fontContexts.end(); ++it) {
            if (*it == context) {
                m_fontContexts.erase(it);
                break;
            }
        }
    });

    context->blockSignals(true);

    context->sendFont(TreelandConfig::ref().fontName());
    context->sendMonospaceFont(TreelandConfig::ref().monoFontName());
    context->sendFontSize(TreelandConfig::ref().fontSize());

    context->blockSignals(false);

    m_fontContexts.push_back(context);
}

void PersonalizationV1::saveImage(personalization_wallpaper_context_v1 *context,
                                  const QString &prefix)
{
    if (!context || context->fd == -1 || m_settingFile.isEmpty()) {
        return;
    }

    QDir dir(m_cacheDirectory);
    if (!dir.exists()) {
        dir.mkpath(m_cacheDirectory);
    }

    QString output = context->output_name;
    if (output.isEmpty()) {
        for (QScreen *screen : QGuiApplication::screens()) {
            output = screen->name();
            break;
        }
    }

    QString dest = m_cacheDirectory + prefix + "_" + output + "_"
        + QDateTime::currentDateTime().toString("yyyyMMddhhmmss");

    QFile src_file;
    if (!src_file.open(context->fd, QIODevice::ReadOnly))
        return;

    QByteArray data = src_file.readAll();
    src_file.close();

    QFile dest_file(dest);
    if (dest_file.open(QIODevice::WriteOnly)) {
        dest_file.write(data);
        dest_file.close();
    }

    QSettings settings(m_settingFile, QSettings::IniFormat);

    // TODO: protocol need to support multi-workspace
    int workspaceId = 1;
    settings.beginGroup(QString("%1.%2.%3").arg(prefix).arg(output).arg(workspaceId));

    const QString &old_path = settings.value("path").toString();
    QFile::remove(old_path);

    settings.setValue("path", dest);
    settings.setValue("isdark", context->isdark);
    settings.endGroup();

    settings.setValue("metadata", context->meta_data);
    m_iniMetaData = context->meta_data;
}

void PersonalizationV1::onWallpaperCommit(personalization_wallpaper_context_v1 *context)
{
    if (context->options & TREELAND_PERSONALIZATION_WALLPAPER_CONTEXT_V1_OPTIONS_BACKGROUND) {
        saveImage(context, "background");
        Q_EMIT backgroundChanged(context->output_name, context->isdark);
    }

    if (context->options & TREELAND_PERSONALIZATION_WALLPAPER_CONTEXT_V1_OPTIONS_LOCKSCREEN) {
        saveImage(context, "lockscreen");
        Q_EMIT lockscreenChanged();
    }
}

void PersonalizationV1::onCursorCommit(personalization_cursor_context_v1 *context)
{
    if (!context->size.isValid() || context->theme.isEmpty()) {
        context->verfity(false);
    }

    setCursorTheme(context->theme);
    setCursorSize(context->size);

    context->verfity(true);
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
    return TreelandConfig::ref().cursorThemeName();
}

void PersonalizationV1::setCursorTheme(const QString &name)
{
    TreelandConfig::ref().setCursorThemeName(name);

    Q_EMIT cursorThemeChanged(name);
}

QSize PersonalizationV1::cursorSize()
{
    int size = TreelandConfig::ref().cursorSize().width();

    return QSize(size, size);
}

void PersonalizationV1::setCursorSize(const QSize &size)
{
    TreelandConfig::ref().setCursorSize(size);

    Q_EMIT cursorSizeChanged(size);
}

int32_t PersonalizationV1::windowRadius() const
{
    return m_dconfig->value("windowRadius", 18).toInt();
}

QString PersonalizationV1::iconTheme() const
{
    return m_dconfig->value("iconThemeName", 18).toString();
}

QString PersonalizationV1::background(const QString &output, int workspaceId)
{
    return readWallpaperSettings("background", output, workspaceId);
}

QString PersonalizationV1::lockscreen(const QString &output, int workspaceId)
{
    return readWallpaperSettings("lockscreen", output, workspaceId);
}

bool PersonalizationV1::backgroundIsDark(const QString &output, int workspaceId)
{
    if (m_settingFile.isEmpty())
        return DEFAULT_WALLPAPER_ISDARK;

    QSettings settings(m_settingFile, QSettings::IniFormat);
    settings.beginGroup(QString("background.%2.%3").arg(output).arg(workspaceId));
    return settings.value("isdark", DEFAULT_WALLPAPER_ISDARK).toBool();
}

bool PersonalizationV1::isAnimagedImage(const QString &source)
{
    QImageReader reader(source);
    return reader.imageCount() > 1;
}

Personalization::Personalization(WToplevelSurface *target,
                                 PersonalizationV1 *manager,
                                 SurfaceWrapper *parent)
    : QObject(parent)
    , m_target(target)
    , m_manager(manager)
{
    connect(target, &WToplevelSurface::aboutToBeInvalidated, this, [this] {
        disconnect(m_connection);
    });

    auto update = [this](personalization_window_context_v1 *context) {
        assert(context);

        if (WSurface::fromHandle(context->surface) != m_target->surface()) {
            return;
        }

        disconnect(m_connection);

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

        m_backgroundType = context->background_type;
        m_cornerRadius = context->corner_radius;
        m_shadow = context->shadow;
        m_border = context->border;
        m_states = context->states;
    };

    m_connection = connect(m_manager, &PersonalizationV1::windowContextCreated, this, update);

    if (auto *context = m_manager->getWindowContext(m_target->surface())) {
        update(context);
    }
}

SurfaceWrapper *Personalization::surfaceWrapper() const
{
    return qobject_cast<SurfaceWrapper*>(parent());
}

Personalization::BackgroundType Personalization::backgroundType() const
{
    return static_cast<Personalization::BackgroundType>(m_backgroundType);
}

bool Personalization::noTitlebar() const
{
    if (qobject_cast<WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface *>(m_target)) {
        return true;
    }

    return m_states.testFlag(personalization_window_context_v1::NoTitleBar);
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
    connect(m_manager,
            &treeland_personalization_manager_v1::fontContextCreated,
            this,
            &PersonalizationV1::onFontContextCreated);
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

QString PersonalizationV1::defaultWallpaper() const
{
    return DEFAULT_WALLPAPER;
}
