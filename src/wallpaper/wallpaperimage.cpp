// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperimage.h"

#include "core/qmlengine.h"
#include "seat/helper.h"
#include "modules/personalization/personalizationmanager.h"
#include "greeter/usermodel.h"
#include "wallpapermanager.h"
#include "workspace/workspacemodel.h"
#include <woutputitem.h>

#include <woutput.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(wallpaperImage, "treeland.wallpaperimage")

WAYLIB_SERVER_USE_NAMESPACE

WallpaperImage::WallpaperImage(QQuickItem *parent)
    : QQuickAnimatedImage(parent)
{
    connect(Helper::instance()->qmlEngine()->singletonInstance<UserModel *>("Treeland",
                                                                            "UserModel"),
            &UserModel::currentUserNameChanged,
            this,
            &WallpaperImage::updateSource);

    connect(Helper::instance()->personalization(),
            &PersonalizationV1::backgroundChanged,
            this,
            &WallpaperImage::updateSource);

    setFillMode(Tile);
    setCache(false);
    setAsynchronous(true);

    updateSource();
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
    if (!m_output) {
        return;
    }

    auto *personalization = Helper::instance()->personalization();
    auto path = personalization->background(m_output->name());
    QUrl url;
    if (path.startsWith("qrc:")) {
        url = QUrl(path);
    } else if (path.startsWith("/")) {
        url = QUrl::fromLocalFile(path);
    }
    setSource(url.toString());
    update();
}
