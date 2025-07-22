// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "output.h"
#include "qmlengine.h"
#include "surfacewrapper.h"
#include "wallpaperprovider.h"
#include "workspace.h"

#include <woutputitem.h>

#include <QQuickItem>

QmlEngine::QmlEngine(QObject *parent)
    : QQmlApplicationEngine(parent)
    , titleBarComponent(this, "Tinywl", "TitleBar")
    , decorationComponent(this, "Tinywl", "Decoration")
    , windowMenuComponent(this, "Tinywl", "WindowMenu")
    , taskBarComponent(this, "Tinywl", "TaskBar")
    , surfaceContent(this, "Tinywl", "SurfaceContent")
    , shadowComponent(this, "Tinywl", "Shadow")
    , geometryAnimationComponent(this, "Tinywl", "GeometryAnimation")
    , menuBarComponent(this, "Tinywl", "OutputMenuBar")
    , workspaceSwitcher(this, "Tinywl", "WorkspaceSwitcher")
{
}

QQuickItem *QmlEngine::createTitleBar(SurfaceWrapper *surface, QQuickItem *parent)
{
    auto context = qmlContext(parent);
    auto obj = titleBarComponent.beginCreate(context);
    titleBarComponent.setInitialProperties(obj, {
        {"surface", QVariant::fromValue(surface)}
    });
    auto item = qobject_cast<QQuickItem*>(obj);
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    titleBarComponent.completeCreate();

    return item;
}

QQuickItem *QmlEngine::createDecoration(SurfaceWrapper *surface, QQuickItem *parent)
{
    auto context = qmlContext(parent);
    auto obj = decorationComponent.beginCreate(context);
    decorationComponent.setInitialProperties(obj, {
        {"surface", QVariant::fromValue(surface)}
    });
    auto item = qobject_cast<QQuickItem*>(obj);
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    decorationComponent.completeCreate();

    return item;
}

QObject *QmlEngine::createWindowMenu(QObject *parent)
{
    auto context = qmlContext(parent);
    auto obj = windowMenuComponent.beginCreate(context);

    obj->setParent(parent);
    windowMenuComponent.completeCreate();

    return obj;
}

QQuickItem *QmlEngine::createBorder(SurfaceWrapper *surface, QQuickItem *parent)
{
    auto context = qmlContext(parent);
    auto obj = borderComponent.beginCreate(context);
    borderComponent.setInitialProperties(obj, {
        {"surface", QVariant::fromValue(surface)}
    });
    auto item = qobject_cast<QQuickItem*>(obj);
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    borderComponent.completeCreate();

    return item;
}

QQuickItem *QmlEngine::createTaskBar(Output *output, QQuickItem *parent)
{
    auto context = qmlContext(parent);
    auto obj = taskBarComponent.beginCreate(context);
    taskBarComponent.setInitialProperties(obj, {
        {"output", QVariant::fromValue(output)}
    });
    auto item = qobject_cast<QQuickItem*>(obj);
    qDebug() << taskBarComponent.errorString();
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    taskBarComponent.completeCreate();

    return item;
}

QQuickItem *QmlEngine::createShadow(QQuickItem *parent)
{
    auto context = qmlContext(parent);
    auto obj = shadowComponent.beginCreate(context);
    auto item = qobject_cast<QQuickItem*>(obj);
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    shadowComponent.completeCreate();

    return item;
}

QQuickItem *QmlEngine::createGeometryAnimation(SurfaceWrapper *surface,
                                               const QRectF &startGeo,
                                               const QRectF &endGeo,
                                               QQuickItem *parent)
{
    auto context = qmlContext(parent);
    auto obj = geometryAnimationComponent.beginCreate(context);
    geometryAnimationComponent.setInitialProperties(obj, {
        {"surface", QVariant::fromValue(surface)},
        {"fromGeometry", QVariant::fromValue(startGeo)},
        {"toGeometry", QVariant::fromValue(endGeo)},
    });
    auto item = qobject_cast<QQuickItem*>(obj);
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    geometryAnimationComponent.completeCreate();

    return item;
}

QQuickItem *QmlEngine::createMenuBar(WOutputItem *output, QQuickItem *parent)
{
    auto context = qmlContext(parent);
    auto obj = menuBarComponent.beginCreate(context);
    menuBarComponent.setInitialProperties(obj, {
        {"output", QVariant::fromValue(output)}
    });
    auto item = qobject_cast<QQuickItem*>(obj);
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    menuBarComponent.completeCreate();

    return item;
}

QQuickItem *QmlEngine::createWorkspaceSwitcher(Workspace *parent, WorkspaceModel *from, WorkspaceModel *to)
{
    auto context = qmlContext(parent);
    auto obj = workspaceSwitcher.beginCreate(context);
    workspaceSwitcher.setInitialProperties(obj, {
        {"parent", QVariant::fromValue(parent)},
        {"from", QVariant::fromValue(from)},
        {"to", QVariant::fromValue(to)},
    });
    auto item = qobject_cast<QQuickItem*>(obj);
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    workspaceSwitcher.completeCreate();

    return item;
}

WallpaperImageProvider *QmlEngine::wallpaperImageProvider()
{
    if (!wallpaperProvider) {
        wallpaperProvider = new WallpaperImageProvider;
        Q_ASSERT(!this->imageProvider("wallpaper"));
        addImageProvider("wallpaper", wallpaperProvider);
    }

    return wallpaperProvider;
}
