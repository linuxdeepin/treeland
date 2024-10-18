// Copyright (C) 2024 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperprovider.h"

#include "helper.h"
#include "personalizationmanager.h"
#include "workspace.h"
#include "workspacemodel.h"

#include <woutput.h>

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QQuickWindow>
#include <QSGTexture>
#include <QStandardPaths>

struct WallpaperData
{
    QString uid;
    QString output;
    QString workspace;

    static WallpaperData fromString(const QString &id)
    {
        const QStringList components = id.split("/");
        if (components.size() != 3) {
            return WallpaperData{};
        }

        return WallpaperData{
            .uid = components[0],
            .output = components[1],
            .workspace = components[2],
        };
    }

    QString toString() { return uid + "/" + output + "/" + workspace; }
};

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
    auto *personalization = Helper::instance()->personalization();
    connect(personalization,
            &PersonalizationV1::backgroundChanged,
            this,
            [this](const QString &output) {
                auto *workspace = Helper::instance()->workspace();
                Q_EMIT wallpaperTextureUpdate(source(output, workspace->current()->name()));
            });
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

QString WallpaperImageProvider::source(WAYLIB_SERVER_NAMESPACE::WOutput *output,
                                       WorkspaceModel *workspace)
{
    if (output && workspace) {
        return source(output->name(), workspace->name());
    }

    return {};
}

QString WallpaperImageProvider::source(const QString &output, const QString &workspace)
{
    // NOTE: Query the actual wallpaper data from the protocol implementation
    auto *personalization = Helper::instance()->personalization();
    return personalization->background(output);
}

QQuickTextureFactory *WallpaperImageProvider::requestTexture(const QString &id,
                                                             QSize *size,
                                                             const QSize &requestedSize)
{
    QQuickTextureFactory *factory = nullptr;
    QSize readSize;

    QString img_path = id;
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
        Q_EMIT wallpaperTextureUpdate(id);
    }

    return factory;
}
