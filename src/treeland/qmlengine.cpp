// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qmlengine.h"

#include "output.h"
#include "surfacewrapper.h"
#include "wallpaperprovider.h"
#include "workspace.h"

#include <woutput.h>
#include <woutputitem.h>

#include <QQuickItem>

Q_LOGGING_CATEGORY(qLcTreelandEngine, "treeland.qml.engine", QtWarningMsg)

QmlEngine::QmlEngine(QObject *parent)
    : QQmlApplicationEngine(parent)
    , titleBarComponent(this, "Treeland", "TitleBar")
    , decorationComponent(this, "Treeland", "Decoration")
    , windowMenuComponent(this, "Treeland", "WindowMenu")
    , taskBarComponent(this, "Treeland", "TaskBar")
    , surfaceContent(this, "Treeland", "SurfaceContent")
    , shadowComponent(this, "Treeland", "Shadow")
    , taskSwitchComponent(this, "Treeland", "TaskSwitcher")
    , geometryAnimationComponent(this, "Treeland", "GeometryAnimation")
    , menuBarComponent(this, "Treeland", "OutputMenuBar")
    , workspaceSwitcher(this, "Treeland", "WorkspaceSwitcher")
    , newAnimationComponent(this, "Treeland", "NewAnimation")
    , lockScreenComponent(this, "Treeland.Greeter", "Greeter")
    , dockPreviewComponent(this, "Treeland", "DockPreview")
    , minimizeAnimationComponent(this, "Treeland", "MinimizeAnimation")
    , showDesktopAnimatioComponentn(this, "Treeland", "ShowDesktopAnimation")
{
}

QQuickItem *QmlEngine::createTitleBar(SurfaceWrapper *surface, QQuickItem *parent)
{
    auto context = qmlContext(parent);
    auto obj = titleBarComponent.beginCreate(context);
    titleBarComponent.setInitialProperties(obj, { { "surface", QVariant::fromValue(surface) } });
    auto item = qobject_cast<QQuickItem *>(obj);
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
    decorationComponent.setInitialProperties(obj, { { "surface", QVariant::fromValue(surface) } });
    auto item = qobject_cast<QQuickItem *>(obj);
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
    if (!obj)
        qCFatal(qLcTreelandEngine) << "Can't create WindowMenu:"
                                   << dockPreviewComponent.errorString();
    obj->setParent(parent);
    windowMenuComponent.completeCreate();

    return obj;
}

QQuickItem *QmlEngine::createBorder(SurfaceWrapper *surface, QQuickItem *parent)
{
    auto context = qmlContext(parent);
    auto obj = borderComponent.beginCreate(context);
    borderComponent.setInitialProperties(obj, { { "surface", QVariant::fromValue(surface) } });
    auto item = qobject_cast<QQuickItem *>(obj);
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
    taskBarComponent.setInitialProperties(obj, { { "output", QVariant::fromValue(output) } });
    auto item = qobject_cast<QQuickItem *>(obj);
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
    auto item = qobject_cast<QQuickItem *>(obj);
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    shadowComponent.completeCreate();

    return item;
}

QQuickItem *QmlEngine::createTaskSwitcher(Output *output, QQuickItem *parent)
{
    auto context = qmlContext(parent);
    auto obj = taskSwitchComponent.beginCreate(context);
    taskSwitchComponent.setInitialProperties(obj, { { "output", QVariant::fromValue(output) } });

    auto item = qobject_cast<QQuickItem *>(obj);
    qDebug() << taskSwitchComponent.errorString();
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    taskSwitchComponent.completeCreate();

    return item;
}

QQuickItem *QmlEngine::createGeometryAnimation(SurfaceWrapper *surface,
                                               const QRectF &startGeo,
                                               const QRectF &endGeo,
                                               QQuickItem *parent)
{
    auto context = qmlContext(parent);
    auto obj = geometryAnimationComponent.beginCreate(context);
    geometryAnimationComponent.setInitialProperties(
        obj,
        {
            { "surface", QVariant::fromValue(surface) },
            { "fromGeometry", QVariant::fromValue(startGeo) },
            { "toGeometry", QVariant::fromValue(endGeo) },
        });
    auto item = qobject_cast<QQuickItem *>(obj);
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
    menuBarComponent.setInitialProperties(obj, { { "output", QVariant::fromValue(output) } });
    auto item = qobject_cast<QQuickItem *>(obj);
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    menuBarComponent.completeCreate();

    return item;
}

QQuickItem *QmlEngine::createWorkspaceSwitcher(Workspace *parent,
                                               WorkspaceModel *from,
                                               WorkspaceModel *to)
{
    auto context = qmlContext(parent);
    auto obj = workspaceSwitcher.beginCreate(context);
    workspaceSwitcher.setInitialProperties(obj,
                                           {
                                               { "parent", QVariant::fromValue(parent) },
                                               { "from", QVariant::fromValue(from) },
                                               { "to", QVariant::fromValue(to) },
                                           });
    auto item = qobject_cast<QQuickItem *>(obj);
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    workspaceSwitcher.completeCreate();

    return item;
}

QQuickItem *QmlEngine::createNewAnimation(SurfaceWrapper *surface,
                                          QQuickItem *parent,
                                          uint direction)
{
    auto context = qmlContext(parent);
    auto obj = newAnimationComponent.beginCreate(context);
    newAnimationComponent.setInitialProperties(obj,
                                               {
                                                   { "target", QVariant::fromValue(surface) },
                                                   { "direction", QVariant::fromValue(direction) },
                                               });
    auto item = qobject_cast<QQuickItem *>(obj);
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    newAnimationComponent.completeCreate();
    return item;
}

QQuickItem *QmlEngine::createDockPreview(QObject *parent)
{
    auto context = qmlContext(parent);
    auto obj = dockPreviewComponent.beginCreate(context);
    auto item = qobject_cast<QQuickItem *>(obj);
    if (!item)
        qCFatal(qLcTreelandEngine) << "Can't create DockPreview:"
                                   << dockPreviewComponent.errorString();
    item->setParent(parent);
    dockPreviewComponent.completeCreate();
    return item;
}

QQuickItem *QmlEngine::createLockScreen(Output *output, QQuickItem *parent)
{
    auto context = qmlContext(parent);
    auto obj = lockScreenComponent.beginCreate(context);
    lockScreenComponent.setInitialProperties(
        obj,
        { { "output", QVariant::fromValue(output->output()) },
          { "outputItem", QVariant::fromValue(output->outputItem()) } });
    auto item = qobject_cast<QQuickItem *>(obj);
    Q_ASSERT_X(item, "", lockScreenComponent.errorString().toUtf8());
    item->setParent(parent);
    item->setParentItem(parent);
    lockScreenComponent.completeCreate();

    return item;
}

QQuickItem *QmlEngine::createMinimizeAnimation(SurfaceWrapper *surface,
                                    QQuickItem *parent,
                                    const QRectF &iconGeometry,
                                    uint direction)
{
    auto context = qmlContext(parent);
    auto obj = minimizeAnimationComponent.beginCreate(context);
    minimizeAnimationComponent.setInitialProperties(obj,
                                                    {
                                                        { "target", QVariant::fromValue(surface) },
                                                        { "position", QVariant::fromValue(iconGeometry) },
                                                        { "direction", QVariant::fromValue(direction) },
                                                    });
    auto item = qobject_cast<QQuickItem *>(obj);
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    minimizeAnimationComponent.completeCreate();

    return item;
}

QQuickItem *QmlEngine::createShowDesktopAnimation(SurfaceWrapper *surface, QQuickItem *parent, bool show)
{
    auto context = qmlContext(parent);
    auto obj = showDesktopAnimatioComponentn.beginCreate(context);
    showDesktopAnimatioComponentn.setInitialProperties(obj,
                                                       {
                                                           { "target", QVariant::fromValue(surface) },
                                                           { "showDesktop", QVariant::fromValue(show) },
                                                       });
    auto item = qobject_cast<QQuickItem *>(obj);
    Q_ASSERT(item);
    item->setParent(parent);
    item->setParentItem(parent);
    showDesktopAnimatioComponentn.completeCreate();

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
