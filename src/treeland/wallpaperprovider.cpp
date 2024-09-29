// Copyright (C) 2024 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperprovider.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QQuickWindow>
#include <QSGTexture>
#include <QStandardPaths>

WallpaperTextureFactory::WallpaperTextureFactory(WallpaperImageProvider *provider,
                                                 const QImage &image)
    : m_wallpaperProvider(provider)
{
    if (image.format() == QImage::Format_ARGB32_Premultiplied
        || image.format() == QImage::Format_RGB32) {
        im = image;
    } else {
        im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    size = im.size();
}

WallpaperTextureFactory::WallpaperTextureFactory(WallpaperImageProvider *provider,
                                                 const QString &id)
    : m_wallpaperProvider(provider)
    , m_wallpaperId(id)
{
    m_textureExist = true;
}

QSGTexture *WallpaperTextureFactory::createTexture(QQuickWindow *window) const
{
    if (m_textureExist)
        m_texture = m_wallpaperProvider->getExistTexture(m_wallpaperId);

    if (!m_texture) {
        m_texture = window->createTextureFromImage(im, QQuickWindow::TextureCanUseAtlas);
        const_cast<WallpaperTextureFactory *>(this)->im = QImage();
    }

    return m_texture;
}

WallpaperImageProvider::WallpaperImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Texture)
{
}

WallpaperImageProvider::~WallpaperImageProvider()
{
    textureCache.clear();
}

QImage WallpaperImageProvider::loadFile(const QString &path, const QSize &requestedSize)
{
    QImageReader imgio(path);
    QSize realSize = imgio.size();

    if (requestedSize.isValid() && requestedSize.width() < realSize.width()
        && requestedSize.height() < realSize.height())
        imgio.setScaledSize(requestedSize);

    QImage image;
    imgio.read(&image);
    return image;
}

QSGTexture *WallpaperImageProvider::getExistTexture(const QString &id) const
{
    if (textureCache.contains(id)) {
        return dynamic_cast<WallpaperTextureFactory *>(textureCache[id].data())->texture();
    }

    return nullptr;
}

QString WallpaperImageProvider::parseFilePath(const QString &id)
{
    QStringList components = id.split("/");
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString local_path = QString("%1/.cache/treeland/wallpaper/%2/%3/%4/")
                             .arg(home)
                             .arg(components[0])
                             .arg(components[1])
                             .arg(components[2]);

    QDir dir(local_path);
    QFileInfo fi;
    QString img_path;
    if (dir.exists()) {
        dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
        QFileInfoList filelist = dir.entryInfoList();

        fi = filelist.first();
        img_path = fi.absoluteFilePath();
    }

    if (!(fi.exists() && fi.isFile())) {
        img_path = ":/qt/qml/Treeland/res/xx.jpg";
    }

    return img_path;
}

QQuickTextureFactory *WallpaperImageProvider::requestTexture(const QString &id,
                                                             QSize *size,
                                                             const QSize &requestedSize)
{
    QQuickTextureFactory *factory = nullptr;
    QSize readSize;

    QString img_path = parseFilePath(id);
    if (textureCache.contains(img_path)) {
        auto cache_factory = textureCache[img_path];

        if (!cache_factory.isNull()) {
            readSize = cache_factory->textureSize();
            if (requestedSize.width() < readSize.width()
                && requestedSize.height() < readSize.height()) {
                *size = readSize;
                return new WallpaperTextureFactory(this, img_path);
            }
        }
    }

    QFileInfo fi(QDir::root(), img_path);
    QString path = fi.canonicalFilePath();
    if (!path.isEmpty()) {
        QImage img = loadFile(path, requestedSize);
        readSize = img.size();
        if (!img.isNull()) {
            factory = new WallpaperTextureFactory(this, img);
        }
    }

    if (size) {
        *size = readSize;
    }

    if (factory) {
        textureCache.insert(img_path, factory);
        Q_EMIT wallpaperTextureUpdate(img_path, readSize.width() * readSize.height());
    }

    return factory;
}
