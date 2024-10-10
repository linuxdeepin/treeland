// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperimage.h"

#include "helper.h"
#include "wallpapermanager.h"
#include "wallpaperprovider.h"
#include "workspace.h"
#include "woutputitem.h"

#include <woutput.h>

#include <QDir>

WAYLIB_SERVER_USE_NAMESPACE

WallpaperImage::WallpaperImage(QQuickItem *parent)
    : QQuickImage(parent)
{
    auto provider = Helper::instance()->qmlEngine()->wallpaperImageProvider();
    connect(provider,
            &WallpaperImageProvider::wallpaperTextureUpdate,
            this,
            &WallpaperImage::updateWallpaperTexture);

    setFillMode(Tile);
    setCache(false);
    setAsynchronous(true);
}

WallpaperImage::~WallpaperImage() { }

int WallpaperImage::userId()
{
    return m_userId;
}

void WallpaperImage::setUserId(const int id)
{
    if (m_userId != id) {
        m_userId = id;
        Q_EMIT userIdChanged();
        updateSource();
    }
}

WorkspaceContainer *WallpaperImage::workspace()
{
    return m_workspace;
}

void WallpaperImage::setWorkspace(WorkspaceContainer *workspace)
{
    if (m_workspace != workspace) {
        m_workspace = workspace;
        Q_EMIT workspaceChanged();
        updateSource();
    }
}

WOutput *WallpaperImage::output()
{
    return m_output;
}

void WallpaperImage::setOutput(WOutput *output)
{
    if (m_output != output) {
        if (m_output)
            QObject::disconnect(m_output, nullptr, this, nullptr);

        m_output = output;
        Q_EMIT outputChanged();

        if (output) {
            setSourceSize(output->transformedSize());
            connect(output, &WOutput::transformedSizeChanged, this, [this] {
                setSourceSize(m_output->transformedSize());
            });

            WallpaperManager::instance()->add(this, WOutputItem::getOutputItem(output));
        } else {
            WallpaperManager::instance()->remove(this);
        }
        updateSource();
    }
}

void WallpaperImage::updateSource()
{
    if (m_userId == -1 || !m_output || !m_workspace) {
        return;
    }

    QStringList paras;
    paras << QString::number(m_userId) << m_output->name() << m_workspace->name();
    QString source = "image://wallpaper/" + paras.join("/");
    setSource(source);
}

void WallpaperImage::updateWallpaperTexture(const QString &id, int size)
{
    QString item_id = source().toString().remove("image://wallpaper/");
    int item_size = sourceSize().width() * sourceSize().height();
    if (item_size < size && item_id == id) {
        load();
        update();
    }
}
