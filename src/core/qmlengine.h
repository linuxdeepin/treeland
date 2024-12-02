// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QQmlApplicationEngine>
#include <QQmlComponent>

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
class CaptureManagerV1;

class QmlEngine : public QQmlApplicationEngine
{
    Q_OBJECT
public:
    explicit QmlEngine(QObject *parent = nullptr);

    QQuickItem *createComponent(QQmlComponent &component,
                                QQuickItem *parent,
                                const QVariantMap &properties = QVariantMap());
    QQuickItem *createTitleBar(SurfaceWrapper *surface, QQuickItem *parent);
    QQuickItem *createDecoration(SurfaceWrapper *surface, QQuickItem *parent);
    QObject *createWindowMenu(QObject *parent);
    QQuickItem *createBorder(SurfaceWrapper *surface, QQuickItem *parent);
    QQuickItem *createTaskBar(Output *output, QQuickItem *parent);
    QQuickItem *createXdgShadow(QQuickItem *parent);
    QQuickItem *createTaskSwitcher(Output *output, QQuickItem *parent);
    QQuickItem *createGeometryAnimation(SurfaceWrapper *surface,
                                        const QRectF &startGeo,
                                        const QRectF &endGeo,
                                        QQuickItem *parent);
    QQuickItem *createMenuBar(WOutputItem *output, QQuickItem *parent);
    QQuickItem *createWorkspaceSwitcher(Workspace *parent);
    QQuickItem *createNewAnimation(SurfaceWrapper *surface, QQuickItem *parent, uint direction);
    QQuickItem *createLaunchpadAnimation(SurfaceWrapper *surface,
                                         uint direction,
                                         QQuickItem *parent);
    QQuickItem *createLaunchpadCover(SurfaceWrapper *surface, Output *output, QQuickItem *parent);
    QQuickItem *createLayerShellAnimation(SurfaceWrapper *surface, QQuickItem *parent, uint direction);
    QQuickItem *createLockScreen(Output *output, QQuickItem *parent);
    QQuickItem *createMinimizeAnimation(SurfaceWrapper *surface,
                                        QQuickItem *parent,
                                        const QRectF &iconGeometry,
                                        uint direction);
    QQuickItem *createDockPreview(QQuickItem *parent);
    QQuickItem *createShowDesktopAnimation(SurfaceWrapper *surface, QQuickItem *parent, bool show);
    QQuickItem *createCaptureSelector(QQuickItem *parent, CaptureManagerV1 *captureManager);
    QQuickItem *createWindowPicker(QQuickItem *parent);

    QQmlComponent *surfaceContentComponent()
    {
        return &surfaceContent;
    }

    WallpaperImageProvider *wallpaperImageProvider();

private:
    QQmlComponent titleBarComponent;
    QQmlComponent decorationComponent;
    QQmlComponent windowMenuComponent;
    QQmlComponent borderComponent;
    QQmlComponent taskBarComponent;
    QQmlComponent surfaceContent;
    QQmlComponent xdgShadowComponent;
    QQmlComponent taskSwitchComponent;
    QQmlComponent geometryAnimationComponent;
    QQmlComponent menuBarComponent;
    QQmlComponent workspaceSwitcher;
    QQmlComponent newAnimationComponent;
#ifndef DISABLE_DDM
    QQmlComponent lockScreenComponent;
#endif
    QQmlComponent dockPreviewComponent;
    QQmlComponent minimizeAnimationComponent;
    QQmlComponent showDesktopAnimatioComponentn;
    QQmlComponent captureSelectorComponent;
    QQmlComponent windowPickerComponent;
    QQmlComponent launchpadAnimationComponent;
    QQmlComponent launchpadCoverComponent;
    QQmlComponent layershellAnimationComponent;
    WallpaperImageProvider *wallpaperProvider = nullptr;
};
