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

#include <QDebug>
#include <QTimer>
#include <QStandardPaths>

#include <sys/socket.h>
#include <unistd.h>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
}

static QuickPersonalizationManager *PERSONALIZATION_MANAGER = nullptr;

class QuickPersonalizationManagerPrivate : public WObjectPrivate {
public:
    QuickPersonalizationManagerPrivate(QuickPersonalizationManager *qq)
        : WObjectPrivate(qq) {}
    ~QuickPersonalizationManagerPrivate() = default;

    W_DECLARE_PUBLIC(QuickPersonalizationManager)

    TreeLandPersonalizationManager *manager = nullptr;
};

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
    connect(this, &QuickPersonalizationManager::currentUserIdChanged,
            this, [this]() {
                QString wallpaper = currentWallpaper();
                Q_EMIT currentWallpaperChanged(wallpaper);
            });
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
    if (!context)
        return;

    QFileInfo file(context->path);
    if (!file.exists())
        return;

    m_currentUserId = context->uid;
    QString cache_location = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString dest_file = cache_location + QString("/wallpaper/%1/%2").arg(m_currentUserId).arg(file.fileName());

    QString cache_dir = getUserCacheWallpaperPath(m_currentUserId);
    QDir dir(cache_dir);
    if (dir.exists()) {
        QStringList file_list = dir.entryList(QDir::Files);
        foreach (const QString &file, file_list) {
            QFile::remove(dir.filePath(file));
        }
    } else {
        dir.mkpath(cache_dir);
    }

    if (QFile::copy(context->path, cache_dir.append(file.fileName()))) {
        Q_EMIT currentWallpaperChanged(dest_file);
    }
}

void QuickPersonalizationManager::onGetUserWallpaper(personalization_wallpaper_context_v1 *context)
{
    if (!context)
        return;

    QString cache_dir = getUserCacheWallpaperPath(context->uid);
    QDir dir(cache_dir);
    if (!dir.exists())
        return;

    Q_EMIT sendUserwallpapers(context, entryWallpaperFiles(dir));
}

void QuickPersonalizationManager::onBackgroundTypeChanged(PersonalizationWindowContext *context)
{
    auto p = context->handle();
    if (!p)
        return;

    Q_EMIT backgroundTypeChanged(WSurface::fromHandle(p->surface), p->background_type);
}

QString QuickPersonalizationManager::getUserCacheWallpaperPath(uid_t uid)
{
    QString cache_location = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return cache_location.append(QString("/wallpaper/%1/").arg(uid));
}

QStringList QuickPersonalizationManager::entryWallpaperFiles(const QDir &dir)
{
    if (!dir.exists())
        return QStringList();

    QStringList nameFilters{"*.png" ,"*.jpg" ,"*.jpeg" , "*.bmp", "*.gif"};
    QStringList entries = dir.entryList(nameFilters);
    for (QStringList::iterator it = entries.begin(); it != entries.end(); ++it) {
        *it = dir.absolutePath() + "/" + *it;
    }

    return entries;
}

QString QuickPersonalizationManager::currentWallpaper()
{
    QString cache_dir = getUserCacheWallpaperPath(m_currentUserId);
    QDir dir(cache_dir);

    QStringList wallpapers = entryWallpaperFiles(dir);
    if (!wallpapers.isEmpty()) {
        QString default_wallpaper = wallpapers.first();

        if (QFile::exists(default_wallpaper)) {
            return QString("file://%1").arg(default_wallpaper);
        }
    }
    return "file:///usr/share/wallpapers/deepin/desktop.jpg";
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

QuickPersonalizationManagerAttached *QuickPersonalizationManager::qmlAttachedProperties(QObject *target)
{
    if (auto *surface = qobject_cast<WXdgSurface*>(target)) {
        return new QuickPersonalizationManagerAttached(surface->surface(), PERSONALIZATION_MANAGER);
    }

    return nullptr;
}
