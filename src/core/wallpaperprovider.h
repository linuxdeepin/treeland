// Copyright (C) 2024 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QQuickImageProvider>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;
WAYLIB_SERVER_END_NAMESPACE

class WallpaperImageProvider;

class WallpaperTextureFactory : public QQuickTextureFactory
{
    Q_OBJECT
public:
    WallpaperTextureFactory(WallpaperImageProvider *provider, const QImage &i);
    WallpaperTextureFactory(WallpaperImageProvider *provider, const QString &id);

    QSGTexture *createTexture(QQuickWindow *window) const override;

    QSGTexture *texture() const
    {
        return m_texture;
    }

    QSize textureSize() const override
    {
        return size;
    }

    int textureByteCount() const override
    {
        return size.width() * size.height() * 4;
    }

    QImage image() const override
    {
        return im;
    }

private:
    QImage im;
    QSize size;
    QString m_wallpaperId;
    bool m_textureExist = false;
    mutable QSGTexture *m_texture = nullptr;
    WallpaperImageProvider *m_wallpaperProvider = nullptr;
};

class WorkspaceModel;

class WallpaperImageProvider : public QQuickImageProvider
{
    Q_OBJECT
public:
    WallpaperImageProvider();
    ~WallpaperImageProvider();

    QQuickTextureFactory *requestTexture(const QString &id,
                                         QSize *size,
                                         const QSize &requestedSize) override;
    QSGTexture *getExistTexture(const QString &id) const;

    QString source(WAYLIB_SERVER_NAMESPACE::WOutput *output, WorkspaceModel *workspace);

private:
    QImage loadFile(const QString &path, const QSize &requestedSize);
    QString source(const QString &output, const QString &workspace);

Q_SIGNALS:
    void wallpaperTextureUpdate(const QString &id);

private:
    QHash<QString, QPointer<QQuickTextureFactory>> textureCache;
};
