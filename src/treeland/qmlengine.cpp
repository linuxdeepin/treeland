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
    , xdgShadowComponent(this, "Treeland", "XdgShadow")
    , taskSwitchComponent(this, "Treeland", "TaskSwitcher")
    , geometryAnimationComponent(this, "Treeland", "GeometryAnimation")
    , menuBarComponent(this, "Treeland", "OutputMenuBar")
    , workspaceSwitcher(this, "Treeland", "WorkspaceSwitcher")
    , newAnimationComponent(this, "Treeland", "NewAnimation")
    , lockScreenComponent(this, "Treeland.Greeter", "Greeter")
    , dockPreviewComponent(this, "Treeland", "DockPreview")
    , minimizeAnimationComponent(this, "Treeland", "MinimizeAnimation")
    , showDesktopAnimatioComponentn(this, "Treeland", "ShowDesktopAnimation")
    , multitaskViewComponent(this, "Treeland", "MultitaskviewProxy")
    , blurComponent(this, "Treeland", "Blur")
{
}

QQuickItem *QmlEngine::createComponent(QQmlComponent &component,
                                       QQuickItem *parent,
                                       const QVariantMap &properties)
{
    auto context = qmlContext(parent);
    auto obj = component.beginCreate(context);
    if (!properties.isEmpty()) {
        component.setInitialProperties(obj, properties);
    }
    auto item = qobject_cast<QQuickItem *>(obj);
    Q_ASSERT_X(item, __func__, component.errorString().toStdString().c_str());
    if (!item)
        qCFatal(qLcTreelandEngine) << "Can't create component:" << component.errorString();
    item->setParent(parent);
    item->setParentItem(parent);
    component.completeCreate();

    return item;
}

QQuickItem *QmlEngine::createTitleBar(SurfaceWrapper *surface, QQuickItem *parent)
{
    return createComponent(titleBarComponent,
                           parent,
                           { { "surface", QVariant::fromValue(surface) } });
}

QQuickItem *QmlEngine::createDecoration(SurfaceWrapper *surface, QQuickItem *parent)
{
    return createComponent(decorationComponent,
                           parent,
                           { { "surface", QVariant::fromValue(surface) } });
}

QObject *QmlEngine::createWindowMenu(QObject *parent)
{
    auto context = qmlContext(parent);
    auto obj = windowMenuComponent.beginCreate(context);
    if (!obj)
        qCFatal(qLcTreelandEngine)
            << "Can't create WindowMenu:" << dockPreviewComponent.errorString();
    obj->setParent(parent);
    windowMenuComponent.completeCreate();

    return obj;
}

QQuickItem *QmlEngine::createBorder(SurfaceWrapper *surface, QQuickItem *parent)
{
    return createComponent(borderComponent,
                           parent,
                           { { "surface", QVariant::fromValue(surface) } });
}

QQuickItem *QmlEngine::createTaskBar(Output *output, QQuickItem *parent)
{
    return createComponent(taskBarComponent, parent, { { "output", QVariant::fromValue(output) } });
}

QQuickItem *QmlEngine::createXdgShadow(QQuickItem *parent)
{
    return createComponent(xdgShadowComponent, parent);
}

QQuickItem *QmlEngine::createBlur(SurfaceWrapper *surface, QQuickItem *parent)
{
    return createComponent(
        blurComponent,
        parent,
        {
            { "radius", QVariant::fromValue(surface->radius()) },
            { "radiusEnabled",
              QVariant::fromValue(surface->radius() > 0 || !surface->noCornerRadius()) },
        });
}

QQuickItem *QmlEngine::createTaskSwitcher(Output *output, QQuickItem *parent)
{
    return createComponent(taskSwitchComponent,
                           parent,
                           { { "output", QVariant::fromValue(output) } });
}

QQuickItem *QmlEngine::createGeometryAnimation(SurfaceWrapper *surface,
                                               const QRectF &startGeo,
                                               const QRectF &endGeo,
                                               QQuickItem *parent)
{
    return createComponent(geometryAnimationComponent,
                           parent,
                           {
                               { "surface", QVariant::fromValue(surface) },
                               { "fromGeometry", QVariant::fromValue(startGeo) },
                               { "toGeometry", QVariant::fromValue(endGeo) },
                           });
}

QQuickItem *QmlEngine::createMenuBar(WOutputItem *output, QQuickItem *parent)
{
    return createComponent(menuBarComponent, parent, { { "output", QVariant::fromValue(output) } });
}

QQuickItem *QmlEngine::createWorkspaceSwitcher(Workspace *parent)
{
    return createComponent(workspaceSwitcher, parent);
}

QQuickItem *QmlEngine::createNewAnimation(SurfaceWrapper *surface,
                                          QQuickItem *parent,
                                          uint direction)
{
    return createComponent(newAnimationComponent,
                           parent,
                           {
                               { "target", QVariant::fromValue(surface) },
                               { "direction", QVariant::fromValue(direction) },
                           });
}

QQuickItem *QmlEngine::createDockPreview(QQuickItem *parent)
{
    return createComponent(dockPreviewComponent, parent);
}

QQuickItem *QmlEngine::createLockScreen(Output *output, QQuickItem *parent)
{
    return createComponent(lockScreenComponent,
                           parent,
                           { { "output", QVariant::fromValue(output->output()) },
                             { "outputItem", QVariant::fromValue(output->outputItem()) } });
}

QQuickItem *QmlEngine::createMinimizeAnimation(SurfaceWrapper *surface,
                                               QQuickItem *parent,
                                               const QRectF &iconGeometry,
                                               uint direction)
{
    return createComponent(minimizeAnimationComponent,
                           parent,
                           {
                               { "target", QVariant::fromValue(surface) },
                               { "position", QVariant::fromValue(iconGeometry) },
                               { "direction", QVariant::fromValue(direction) },
                           });
}

QQuickItem *QmlEngine::createShowDesktopAnimation(SurfaceWrapper *surface,
                                                  QQuickItem *parent,
                                                  bool show)
{
    return createComponent(showDesktopAnimatioComponentn,
                           parent,
                           {
                               { "target", QVariant::fromValue(surface) },
                               { "showDesktop", QVariant::fromValue(show) },
                           });
}

QQuickItem *QmlEngine::createMultitaskview(QQuickItem *parent)
{
    auto item = createComponent(multitaskViewComponent, parent);
    // Multitaskview should occupy parent and clip children.
    item->setX(0);
    item->setY(0);
    item->setWidth(parent->width());
    item->setHeight(parent->height());
    item->setClip(true);
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
