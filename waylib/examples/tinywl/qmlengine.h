// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QQmlApplicationEngine>
#include <QQmlComponent>

#include <wglobal.h>

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutputItem;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class WallpaperImageProvider;
class SurfaceWrapper;
class Output;
class Workspace;
class WorkspaceModel;
class QmlEngine : public QQmlApplicationEngine
{
    Q_OBJECT
public:
    explicit QmlEngine(QObject *parent = nullptr);

    QQuickItem *createTitleBar(SurfaceWrapper *surface, QQuickItem *parent);
    QQuickItem *createDecoration(SurfaceWrapper *surface, QQuickItem *parent);
    QObject *createWindowMenu(QObject *parent);
    QQuickItem *createBorder(SurfaceWrapper *surface, QQuickItem *parent);
    QQuickItem *createTaskBar(Output *output, QQuickItem *parent);
    QQuickItem *createShadow(QQuickItem *parent);
    QQuickItem *createGeometryAnimation(SurfaceWrapper *surface, const QRectF &startGeo,
                                        const QRectF &endGeo, QQuickItem *parent);
    QQuickItem *createMenuBar(WOutputItem *output, QQuickItem *parent);
    QQuickItem *createWorkspaceSwitcher(Workspace *parent, WorkspaceModel *from, WorkspaceModel *to);
    QQmlComponent *surfaceContentComponent() { return &surfaceContent; }
    WallpaperImageProvider *wallpaperImageProvider();

private:
    QQmlComponent titleBarComponent;
    QQmlComponent decorationComponent;
    QQmlComponent windowMenuComponent;
    QQmlComponent borderComponent;
    QQmlComponent taskBarComponent;
    QQmlComponent surfaceContent;
    QQmlComponent shadowComponent;
    QQmlComponent geometryAnimationComponent;
    QQmlComponent menuBarComponent;
    QQmlComponent workspaceSwitcher;
    WallpaperImageProvider *wallpaperProvider = nullptr;
};
