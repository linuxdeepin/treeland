// Copyright (C) 2024 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once
#include <QQuickImageProvider>

class WallpaperImageProvider;
class WallpaperTextureFactory : public QQuickTextureFactory
{
    Q_OBJECT
public:
    WallpaperTextureFactory(WallpaperImageProvider *provider, const QImage &i);
    WallpaperTextureFactory(WallpaperImageProvider *provider, const QString &id);

    QSGTexture *createTexture(QQuickWindow *window) const override;
    QSGTexture *texture() const { return m_texture; }
    QSize textureSize() const override { return size; }
    int textureByteCount() const override { return size.width() * size.height() * 4; }
    QImage image() const override { return im; }

private:
    QImage im;
    QSize size;
    WallpaperImageProvider *m_wallpaperProvider = nullptr;
    QString m_wallpaperId;
    bool m_textureExist = false;
    mutable QSGTexture *m_texture = nullptr;
};

class WallpaperImageProvider : public QQuickImageProvider
{
    Q_OBJECT
public:
    WallpaperImageProvider();
    ~WallpaperImageProvider();

    QQuickTextureFactory *requestTexture(const QString &id, QSize *size, const QSize &requestedSize) override;
    QSGTexture *getExistTexture(const QString& id) const;

private:
    QImage loadFile(const QString &path, const QSize &requestedSize);
    QString parseFilePath(const QString &id);

Q_SIGNALS:
    void wallpaperTextureUpdate(const QString& id, int size);

private:
    QHash<QString, QPointer<QQuickTextureFactory>> textureCache;
};
