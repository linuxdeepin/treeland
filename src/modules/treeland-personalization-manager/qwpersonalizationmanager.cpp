// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwpersonalizationmanager.h"

#include "impl/personalizationmanager.h"
#include "server-protocol.h"

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwsignalconnector.h>
#include <qwxdgshell.h>

#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

#include <sys/socket.h>
#include <unistd.h>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
}

static QuickPersonalizationManager *PERSONALIZATION_MANAGER = nullptr;
static QQuickItem *PERSONALIZATION_IMAGE = nullptr;

#define DEFAULT_WALLPAPER "file:///usr/share/wallpapers/deepin/desktop.jpg"

class QuickPersonalizationManagerPrivate : public WObjectPrivate
{
public:
    QuickPersonalizationManagerPrivate(QuickPersonalizationManager *qq);
    ~QuickPersonalizationManagerPrivate();

    void updateCacheWallpaperPath(uid_t uid);
    QString readWallpaperSettings(const QString &group, WOutput *w_output);
    void saveWallpaperSettings(const QString &current,
                               personalization_wallpaper_context_v1 *context);
    QString outputName(const WOutput *w_output);

    W_DECLARE_PUBLIC(QuickPersonalizationManager)

    uid_t m_userId = 0;
    QString m_cacheDirectory;
    QString m_settingFile;
    QString m_iniMetaData;

    TreeLandPersonalizationManager *manager = nullptr;
};

QuickPersonalizationManagerPrivate::QuickPersonalizationManagerPrivate(
    QuickPersonalizationManager *qq)
    : WObjectPrivate(qq)
{
}

QuickPersonalizationManagerPrivate::~QuickPersonalizationManagerPrivate() { }

void QuickPersonalizationManagerPrivate::updateCacheWallpaperPath(uid_t uid)
{
    QString cache_location = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    m_cacheDirectory = cache_location + QString("/wallpaper/%1/").arg(uid);
    m_settingFile = m_cacheDirectory + "wallpaper.ini";

    QSettings settings(m_settingFile, QSettings::IniFormat);
    m_iniMetaData = settings.value("metadata").toString();
}

QString QuickPersonalizationManagerPrivate::outputName(const WOutput *w_output)
{
    if (w_output == NULL)
        return QString();

    wlr_output *output = w_output->nativeHandle();
    if (output == NULL)
        return QString();

    return output->name;
}

QString QuickPersonalizationManagerPrivate::readWallpaperSettings(const QString &group,
                                                                  WOutput *w_output)
{
    if (m_settingFile.isEmpty())
        return QString();

    QSettings settings(m_settingFile, QSettings::IniFormat);

    QString value = DEFAULT_WALLPAPER;
    QString output_name = outputName(w_output);
    if (output_name.isEmpty())
        return DEFAULT_WALLPAPER;
    else {
        value = settings.value(group + "/" + output_name).toString();
    }

    if (value.isEmpty())
        return DEFAULT_WALLPAPER;

    return QString("file://%1").arg(value);
}

void QuickPersonalizationManagerPrivate::saveWallpaperSettings(
    const QString &current, personalization_wallpaper_context_v1 *context)
{
    if (m_settingFile.isEmpty() || context == nullptr)
        return;

    QSettings settings(m_settingFile, QSettings::IniFormat);

    W_Q(QuickPersonalizationManager);
    if (context->options & PERSONALIZATION_WALLPAPER_CONTEXT_V1_OPTIONS_BACKGROUND) {
        settings.setValue(QString::asprintf("background/%s", context->output_name), current);
    }

    if (context->options & PERSONALIZATION_WALLPAPER_CONTEXT_V1_OPTIONS_LOCKSCREEN) {
        settings.setValue(QString::asprintf("lockscreen/%s", context->output_name), current);
    }

    settings.setValue("metadata", context->meta_data);
    m_iniMetaData = QString::fromLocal8Bit(context->meta_data);
}

QuickPersonalizationManager::QuickPersonalizationManager(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new QuickPersonalizationManagerPrivate(this), nullptr)
{
    if (PERSONALIZATION_MANAGER) {
        qFatal("There are multiple instances of QuickPersonalizationManager");
    }

    PERSONALIZATION_MANAGER = this;
}

void QuickPersonalizationManager::create()
{
    W_D(QuickPersonalizationManager);
    WQuickWaylandServerInterface::create();

    d->manager = TreeLandPersonalizationManager::create(server()->handle());
    connect(d->manager,
            &TreeLandPersonalizationManager::windowContextCreated,
            this,
            &QuickPersonalizationManager::onWindowContextCreated);
    connect(d->manager,
            &TreeLandPersonalizationManager::wallpaperContextCreated,
            this,
            &QuickPersonalizationManager::onWallpaperContextCreated);
}

void QuickPersonalizationManager::onWindowContextCreated(PersonalizationWindowContext *context)
{
    connect(context,
            &PersonalizationWindowContext::backgroundTypeChanged,
            this,
            &QuickPersonalizationManager::onBackgroundTypeChanged);
}

void QuickPersonalizationManager::onWallpaperContextCreated(
    PersonalizationWallpaperContext *context)
{
    connect(context,
            &PersonalizationWallpaperContext::commit,
            this,
            &QuickPersonalizationManager::onCommit);
    connect(context,
            &PersonalizationWallpaperContext::getWallpapers,
            this,
            &QuickPersonalizationManager::onGetWallpapers);
}

QString QuickPersonalizationManager::saveImage(personalization_wallpaper_context_v1 *context,
                                               const QString prefix)
{
    if (!context || context->fd == -1 || !strlen(context->output_name))
        return QString();

    W_D(QuickPersonalizationManager);

    QFile src_file;
    if (!src_file.open(context->fd, QIODevice::ReadOnly))
        return QString();

    QByteArray data = src_file.readAll();
    src_file.close();

    QString dest_path = d->m_cacheDirectory + prefix + "_" + context->output_name;

    QDir dir(d->m_cacheDirectory);
    if (!dir.exists()) {
        dir.mkpath(d->m_cacheDirectory);
    }

    QFile dest_file(dest_path);
    if (dest_file.open(QIODevice::WriteOnly)) {
        dest_file.write(data);
        dest_file.close();

        d->saveWallpaperSettings(dest_path, context);
        return dest_path;
    }

    return QString();
}

void QuickPersonalizationManager::onCommit(personalization_wallpaper_context_v1 *context)
{
    if (context->options & PERSONALIZATION_WALLPAPER_CONTEXT_V1_OPTIONS_BACKGROUND) {
        QString background = saveImage(context, "background");
        if (!background.isEmpty()) {
            Q_EMIT backgroundChanged();
        }
    }

    if (context->options & PERSONALIZATION_WALLPAPER_CONTEXT_V1_OPTIONS_LOCKSCREEN) {
        QString lockscreen = saveImage(context, "lockscreen");
        if (!lockscreen.isEmpty()) {
            Q_EMIT backgroundChanged();
        }
    }
}

void QuickPersonalizationManager::onGetWallpapers(personalization_wallpaper_context_v1 *context)
{
    if (!context)
        return;

    W_D(QuickPersonalizationManager);

    QDir dir(d->m_cacheDirectory);
    if (!dir.exists())
        return;

    context->meta_data = d->m_iniMetaData.toLocal8Bit();
    personalization_wallpaper_context_v1_send_metadata(context->resource, context->meta_data);
}

void QuickPersonalizationManager::onBackgroundTypeChanged(PersonalizationWindowContext *context)
{
    auto p = context->handle();
    if (!p)
        return;

    Q_EMIT backgroundTypeChanged(WSurface::fromHandle(p->surface), p->background_type);
}

uid_t QuickPersonalizationManager::userId()
{
    W_D(QuickPersonalizationManager);
    return d->m_userId;
}

void QuickPersonalizationManager::setUserId(uid_t uid)
{
    W_D(QuickPersonalizationManager);

    d->m_userId = uid;
    d->updateCacheWallpaperPath(uid);
    Q_EMIT userIdChanged(uid);
}

QString QuickPersonalizationManager::background(WOutput *w_output)
{
    W_D(QuickPersonalizationManager);
    return d->readWallpaperSettings("background", w_output);
}

QString QuickPersonalizationManager::lockscreen(WOutput *w_output)
{
    W_D(QuickPersonalizationManager);
    return d->readWallpaperSettings("lockscreen", w_output);
}

QuickPersonalizationManagerAttached::QuickPersonalizationManagerAttached(
    WSurface *target, QuickPersonalizationManager *manager)
    : QObject(manager)
    , m_target(target)
    , m_manager(manager)
{
    connect(m_manager,
            &QuickPersonalizationManager::backgroundTypeChanged,
            this,
            [this](WSurface *surface, uint32_t type) {
                if (m_target == surface) {
                    m_backgroundType = static_cast<BackgroundType>(type);
                    Q_EMIT backgroundTypeChanged();
                }
            });
}

QuickPersonalizationManagerAttached::QuickPersonalizationManagerAttached(
    QQuickItem *target, QuickPersonalizationManager *manager)
    : QObject(manager)
    , m_target(target)
    , m_manager(manager)
{
    if (PERSONALIZATION_IMAGE != nullptr) {
        qFatal("Must set image once!");
    }
    PERSONALIZATION_IMAGE = target;
}

QQuickItem *QuickPersonalizationManagerAttached::backgroundImage() const
{
    return PERSONALIZATION_IMAGE;
}

QuickPersonalizationManagerAttached *QuickPersonalizationManager::qmlAttachedProperties(
    QObject *target)
{
    if (auto *surface = qobject_cast<WXdgSurface *>(target)) {
        return new QuickPersonalizationManagerAttached(surface->surface(), PERSONALIZATION_MANAGER);
    }

    if (auto *image = qobject_cast<QQuickItem *>(target)) {
        return new QuickPersonalizationManagerAttached(image, PERSONALIZATION_MANAGER);
    }
    return nullptr;
}
