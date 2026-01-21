// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "loggings.h"
#include "treelandwallpapernotifierclient.h"
#include "mpvvideoitem.h"
#include "wallpaperwindow.h"

#include <private/qquickanimatedimage_p.h>

#define TREELANDWALLPAPERPRODUCEV1VERSION 1

static QSize maxScreenSize()
{
    QSize maxSize;
    int maxArea = 0;

    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        const QSize size = screen->geometry().size();
        const int area = size.width() * size.height();
        if (area > maxArea) {
            maxArea = area;
            maxSize = size;
        }
    }
    return maxSize;
}

TreelandWallpaperNotifierClientV1::TreelandWallpaperNotifierClientV1()
    : QWaylandClientExtensionTemplate<TreelandWallpaperNotifierClientV1>(TREELANDWALLPAPERPRODUCEV1VERSION)
{
    connect(qApp, &QGuiApplication::screenAdded,
            this, &TreelandWallpaperNotifierClientV1::onScreenAdded);
    connect(qApp, &QGuiApplication::screenRemoved,
            this, &TreelandWallpaperNotifierClientV1::onScreenRemoved);
    for (QScreen *screen : QGuiApplication::screens()) {
         m_connectedScreens.insert(screen);
        connect(screen, &QScreen::geometryChanged,
                this, &TreelandWallpaperNotifierClientV1::updateAllWallpaperViewSizes);
    }
}

TreelandWallpaperNotifierClientV1::~TreelandWallpaperNotifierClientV1()
{
    foreach (auto window, m_windows) {
            delete window;
    }
    m_windows.clear();
    m_connectedScreens.clear();

    destroy();
}

void TreelandWallpaperNotifierClientV1::instantiate()
{
    initialize();
}

void TreelandWallpaperNotifierClientV1::treeland_wallpaper_notifier_v1_add(uint32_t source_type, const QString &file_source)
{
    QQuickView *wallpaperWindow = new QQuickView;
    WallpaperWindow *window = WallpaperWindow::get(wallpaperWindow);
    window->setSource(file_source);
    wallpaperWindow->setResizeMode(QQuickView::SizeRootObjectToView);
    switch (source_type) {

    case QtWayland::treeland_wallpaper_notifier_v1::
        wallpaper_source_type::wallpaper_source_type_image: {
        wallpaperWindow->loadFromModule("com.treeland.wallfactory", "Image");
        QObject *root = wallpaperWindow->rootObject();
        auto *image = qobject_cast<QQuickAnimatedImage *>(root);
        if (!image) {
            qCCritical(WALLPAPER)
            << "Root object is not QQuickAnimatedImage";
            delete wallpaperWindow;
            return;
        }

        image->setSource(QUrl::fromLocalFile(file_source));
        break;
    }

    case QtWayland::treeland_wallpaper_notifier_v1::
        wallpaper_source_type::wallpaper_source_type_video: {
        wallpaperWindow->loadFromModule("com.treeland.wallfactory", "Video");
        QObject *root = wallpaperWindow->rootObject();
        auto *video = qobject_cast<MpvVideoItem *>(root);
        if (!video) {
            qCCritical(WALLPAPER)
            << "Root object is not MpvVideoItem";
            delete wallpaperWindow;
            return;
        }

        video->setSource(file_source);
        break;
    }

    default:
        qCCritical(WALLPAPER) << "Unsupported wallpaper source type:"
                              << source_type;
        delete wallpaperWindow;
        return;
    }

    wallpaperWindow->resize(maxScreenSize());
    wallpaperWindow->show();
    m_windows.append(wallpaperWindow);
}

void TreelandWallpaperNotifierClientV1::treeland_wallpaper_notifier_v1_remove(const QString &file_source)
{
    foreach (auto window, m_windows) {
        if (window->source() == file_source) {
            m_windows.removeOne(window);
            delete window;

            return;
        }
    }
}

void TreelandWallpaperNotifierClientV1::updateAllWallpaperViewSizes()
{
    const QSize size = maxScreenSize();
    if (!size.isValid())
        return;

    for (QQuickView *view : std::as_const(m_windows)) {
        if (!view)
            continue;

        view->resize(size);
    }
}

void TreelandWallpaperNotifierClientV1::onScreenAdded(QScreen *screen)
{
    if (!m_connectedScreens.contains(screen)) {
        m_connectedScreens.insert(screen);
        connect(screen, &QScreen::geometryChanged,
                this, &TreelandWallpaperNotifierClientV1::updateAllWallpaperViewSizes);
    }

    updateAllWallpaperViewSizes();
}

void TreelandWallpaperNotifierClientV1::onScreenRemoved(QScreen *screen)
{
    if (m_connectedScreens.contains(screen)) {
        m_connectedScreens.remove(screen);
        disconnect(screen, &QScreen::geometryChanged,
                   this, &TreelandWallpaperNotifierClientV1::updateAllWallpaperViewSizes);
    }

    updateAllWallpaperViewSizes();
}
