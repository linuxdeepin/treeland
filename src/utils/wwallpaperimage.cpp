// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperprovider.h"
#include "wwallpaperimage.h"
#include "helper.h"

#include <QDir>

WWallpaperImage::WWallpaperImage(QQuickItem *parent)
    : QQuickImage(parent)
{
    registerProvider();
}

WWallpaperImage::~WWallpaperImage()
{

}

int WWallpaperImage::userId()
{
    return m_userId;

}

void WWallpaperImage::setUserId(const int id)
{
    if (m_userId != id) {
        m_userId = id;
        Q_EMIT userIdChanged();
        updateSource();
    }
}

int WWallpaperImage::workspace()
{
    return m_workspaceId;
}

void WWallpaperImage::setWorkspace(const int id)
{
    if (m_workspaceId != id) {
        m_workspaceId = id;
        Q_EMIT workspaceChanged();
        updateSource();
    }
}

WOutput* WWallpaperImage::output()
{
    return m_output;
}

void WWallpaperImage::setOutput(WOutput* output)
{
    if (m_output != output) {
        m_output = output;
        Q_EMIT outputChanged();
        updateSource();
    }
}

void WWallpaperImage::updateSource()
{
    if (m_userId == -1 ||
        !m_output ||
        m_workspaceId == -1) {
        return;
    }

    QStringList paras;
    paras << QString::number(m_userId) << m_output->name() << QString::number(m_workspaceId);
    QString source = "image://wallpaper/" + paras.join("/");
    setSource(source);
}

void WWallpaperImage::updateWallpaperTexture(const QString& id, int size)
{
    QString item_id = source().toString().remove("image://wallpaper/");
    int item_size = sourceSize().width() * sourceSize().height();
    if (item_size < size && item_id == id) {
        load();
        update();
    }
}

void WWallpaperImage::registerProvider()
{
    QQmlEngine *engine = qmlEngine(this);

    auto provider = qobject_cast<WallpaperImageProvider *>(engine->imageProvider("wallpaper"));
    if (!provider) {
        provider = new WallpaperImageProvider;
        connect(provider, &WallpaperImageProvider::wallpaperTextureUpdate, this, &WWallpaperImage::updateWallpaperTexture);
        engine->addImageProvider("wallpaper", provider);
    }
}
