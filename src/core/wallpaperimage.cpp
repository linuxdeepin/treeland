// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperimage.h"

#include "helper.h"
#include "usermodel.h"
#include "wallpapermanager.h"
#include "wallpaperprovider.h"
#include "workspacemodel.h"
#include "woutputitem.h"

#include <woutput.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(wallpaperImage, "treeland.wallpaperimage")

WAYLIB_SERVER_USE_NAMESPACE

WallpaperImage::WallpaperImage(QQuickItem *parent)
    : QQuickAnimatedImage(parent)
{
    auto provider = Helper::instance()->qmlEngine()->wallpaperImageProvider();
    connect(provider,
            &WallpaperImageProvider::wallpaperTextureUpdate,
            this,
            &WallpaperImage::updateWallpaperTexture);
    connect(Helper::instance()->qmlEngine()->singletonInstance<UserModel *>("Treeland.Greeter",
                                                                            "UserModel"),
            &UserModel::currentUserNameChanged,
            this,
            &WallpaperImage::updateSource);

    setFillMode(Tile);
    setCache(false);
    setAsynchronous(true);
}

WallpaperImage::~WallpaperImage() { }

WorkspaceModel *WallpaperImage::workspace()
{
    return m_workspace;
}

void WallpaperImage::setWorkspace(WorkspaceModel *workspace)
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
    auto *provider = Helper::instance()->qmlEngine()->wallpaperImageProvider();
    setSource(provider->source(m_output, m_workspace));
}

void WallpaperImage::updateWallpaperTexture(const QString &id)
{
    // TODO: optimize this
    if (!source().toString().isEmpty()) {
        QFile::remove(source().toString().remove("file://"));
    }
    updateSource();
    load();
    update();
    setPlaying(true);
}
