// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwpersonalizationmanager.h"

#include "personalization-server-protocol.h"
#include "personalizationmanager.h"

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwsignalconnector.h>
#include <qwxdgshell.h>
#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>

#include <QDir>
#include <QDebug>
#include <QSettings>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>

#include <sys/socket.h>
#include <unistd.h>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
}

static QuickPersonalizationManager *PERSONALIZATION_MANAGER = nullptr;
static QQuickItem *PERSONALIZATION_IMAGE = nullptr;

class QuickPersonalizationManagerPrivate : public WObjectPrivate {
public:
    QuickPersonalizationManagerPrivate(QuickPersonalizationManager *qq);
    ~QuickPersonalizationManagerPrivate();

    void updateCacheWallpaperPath(uid_t uid);
    void loadWallpaperSettings();
    void saveWallpaperSettings(const QString& current, const char* meta);

    W_DECLARE_PUBLIC(QuickPersonalizationManager)

    uid_t m_currentUserId = 0;
    QString m_cacheDirectory;
    QString m_settingFile;
    QString m_currentWallpaper;
    QString m_iniMetaData;

    TreeLandPersonalizationManager *manager = nullptr;
};

QuickPersonalizationManagerPrivate::QuickPersonalizationManagerPrivate(QuickPersonalizationManager *qq)
    : WObjectPrivate(qq)
{
}

QuickPersonalizationManagerPrivate::~QuickPersonalizationManagerPrivate()
{
}

void QuickPersonalizationManagerPrivate::updateCacheWallpaperPath(uid_t uid)
{
    QString cache_location = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    m_cacheDirectory = cache_location + QString("/wallpaper/%1/").arg(uid);
    m_settingFile = m_cacheDirectory + "wallpaper.ini";
    m_currentWallpaper = "/usr/share/wallpapers/deepin/desktop.jpg";
}

void QuickPersonalizationManagerPrivate::loadWallpaperSettings()
{
    if (m_settingFile.isEmpty())
        return;

    QSettings settings(m_settingFile, QSettings::IniFormat);

    W_Q(QuickPersonalizationManager);
    q->setCurrentWallpaper(settings.value("currentwallpaper").toString());
    m_iniMetaData = settings.value("metadata").toString();
}

void QuickPersonalizationManagerPrivate::saveWallpaperSettings(const QString& current, const char* meta)
{
    if (m_settingFile.isEmpty())
        return;

    QSettings settings(m_settingFile, QSettings::IniFormat);

    settings.setValue("currentwallpaper", current);
    settings.setValue("metadata", meta);

    W_Q(QuickPersonalizationManager);
    q->setCurrentWallpaper(current);
    m_iniMetaData = QString::fromLocal8Bit(meta);
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

void QuickPersonalizationManager::create() {
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
    connect(this, &QuickPersonalizationManager::sendUserwallpapers,
            d->manager,
            &TreeLandPersonalizationManager::onSendUserWallpapers);
}

void QuickPersonalizationManager::onWindowContextCreated(PersonalizationWindowContext *context)
{
    connect(context,
            &PersonalizationWindowContext::backgroundTypeChanged,
            this,
            &QuickPersonalizationManager::onBackgroundTypeChanged);
}

void QuickPersonalizationManager::onWallpaperContextCreated(PersonalizationWallpaperContext *context)
{
    connect(context,
            &PersonalizationWallpaperContext::setUserWallpaper,
            this,
            &QuickPersonalizationManager::onSetUserWallpaper);
    connect(context,
            &PersonalizationWallpaperContext::getUserWallpaper,
            this,
            &QuickPersonalizationManager::onGetUserWallpaper);
}

void QuickPersonalizationManager::onSetUserWallpaper(personalization_wallpaper_context_v1 *context)
{
    if (!context || context->fd == -1)
        return;

    W_D(QuickPersonalizationManager);

    QFile src_file;
    if (!src_file.open(context->fd, QIODevice::ReadOnly))
        return;

    QByteArray data = src_file.readAll();
    src_file.close();

    QString dest_path = d->m_cacheDirectory + "desktop";
    QDir dir(d->m_cacheDirectory);
    if (!dir.exists()) {
        dir.mkpath(d->m_cacheDirectory);
    }

    QFile dest_file(dest_path);
    if (dest_file.open(QIODevice::WriteOnly)) {
        dest_file.write(data);
        dest_file.close();

        d->saveWallpaperSettings(dest_path, context->metaData);
    }
}

void QuickPersonalizationManager::onGetUserWallpaper(personalization_wallpaper_context_v1 *context)
{
    if (!context)
        return;

    W_D(QuickPersonalizationManager);

    QDir dir(d->m_cacheDirectory);
    if (!dir.exists())
        return;

    context->metaData = d->m_iniMetaData.toLocal8Bit();
    Q_EMIT sendUserwallpapers(context);
}

void QuickPersonalizationManager::onBackgroundTypeChanged(PersonalizationWindowContext *context)
{
    auto p = context->handle();
    if (!p)
        return;

    Q_EMIT backgroundTypeChanged(WSurface::fromHandle(p->surface), p->background_type);
}

QString QuickPersonalizationManager::currentWallpaper()
{
    W_D(QuickPersonalizationManager);

    if (QFile::exists(d->m_currentWallpaper)) {
        return QString("file://%1").arg(d->m_currentWallpaper);
    }
    return "file:///usr/share/wallpapers/deepin/desktop.jpg";
}

void QuickPersonalizationManager::setCurrentWallpaper(const QString& path)
{
    W_D(QuickPersonalizationManager);

    d->m_currentWallpaper = path;
    Q_EMIT currentWallpaperChanged(path);
}

uid_t QuickPersonalizationManager::currentUserId()
{
    W_D(QuickPersonalizationManager);
    return d->m_currentUserId;
}

void QuickPersonalizationManager::setCurrentUserId(uid_t uid)
{
    W_D(QuickPersonalizationManager);

    d->m_currentUserId = uid;
    d->updateCacheWallpaperPath(uid);
    d->loadWallpaperSettings();

    Q_EMIT currentUserIdChanged(uid);
}

QuickPersonalizationManagerAttached::QuickPersonalizationManagerAttached(WSurface *target, QuickPersonalizationManager *manager)
    : QObject(manager)
    , m_target(target)
    , m_manager(manager)
{
    connect(m_manager, &QuickPersonalizationManager::backgroundTypeChanged, this, [this] (WSurface *surface, uint32_t type) {
        if (m_target == surface) {
            m_backgroundType = static_cast<BackgroundType>(type);
            Q_EMIT backgroundTypeChanged();
        }
    });
}

QuickPersonalizationManagerAttached::QuickPersonalizationManagerAttached(QQuickItem *target, QuickPersonalizationManager *manager)
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

QuickPersonalizationManagerAttached *QuickPersonalizationManager::qmlAttachedProperties(QObject *target)
{
    if (auto *surface = qobject_cast<WXdgSurface*>(target)) {
        return new QuickPersonalizationManagerAttached(surface->surface(), PERSONALIZATION_MANAGER);
    }

    if (auto *image = qobject_cast<QQuickItem*>(target)) {
        return new QuickPersonalizationManagerAttached(image, PERSONALIZATION_MANAGER);
    }
    return nullptr;
}


