// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treelandremotesource.h"

#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "output/output.h"
#include "seat/helper.h"
#include "surface/surfacecontainer.h"
#include "surface/surfacewrapper.h"
#include "workspace/workspace.h"
#include "workspace/workspacemodel.h"

#include <wcursor.h>
#include <woutput.h>
#include <woutputrenderwindow.h>

#include <QLocalServer>
#include <QMetaEnum>
#include <QRemoteObjectHost>
#include <QUrl>

WAYLIB_SERVER_USE_NAMESPACE

TreelandRemoteSource::TreelandRemoteSource(QObject *parent)
    : WindowTreeRemoteSource(parent)
{
    auto *host = new QRemoteObjectHost(this);
    QRemoteObjectHost::setLocalServerOptions(QLocalServer::UserAccessOption);
    host->setHostUrl(QUrl(QStringLiteral("local:org.deepin.dde.treeland.debug")));
    host->enableRemoting(this, QStringLiteral("WindowTree"));

    if (auto *root = Helper::instance()->rootSurfaceContainer(); root && root->cursor()) {
        updateCursor(root->cursor()->position());
    }
}

TreelandRemoteSource::~TreelandRemoteSource() = default;

QPointF TreelandRemoteSource::cursorPosition() const
{
    auto *self = const_cast<TreelandRemoteSource *>(this);
    if (!self->m_cursorTracking) {
        if (auto *root = Helper::instance()->rootSurfaceContainer(); root && root->cursor()) {
            self->m_cursorTracking = true;
            connect(root->cursor(), &WCursor::positionChanged, self, [self, root] {
                if (root->cursor())
                    self->updateCursor(root->cursor()->position());
            });
            self->updateCursor(root->cursor()->position());
        }
    }
    return m_cursorPosition;
}

TreelandInfo TreelandRemoteSource::getTreelandInfo()
{
    TreelandInfo info;

    auto mode = Helper::instance()->currentMode();
    info.setCurrentMode(QString::fromUtf8(QMetaEnum::fromType<Helper::CurrentMode>().valueToKey(static_cast<int>(mode))));

    if (auto *root = Helper::instance()->rootSurfaceContainer()) {
        QList<LayerInfo> layers;
        const auto subcontainers = root->subContainers();
        for (auto *container : subcontainers) {
            if (!container)
                continue;
            layers.append(buildLayerInfo(container));
        }
        info.setLayers(layers);
    }
    return info;
}

WindowInfo TreelandRemoteSource::buildWindowInfo(SurfaceWrapper *surface,
                                                 int layer,
                                                 const QString &containerName,
                                                 int z) const
{
    WindowInfo info;
    info.setAppId(surface->appId());
    info.setWorkspace(surface->workspaceId());
    info.setLayer(layer);
    info.setZ(z);
    info.setContainer(containerName);
    info.setGeometry(surface->geometry());
    info.setTitlebarGeometry(surface->titlebarGeometry());
    info.setBoundingRect(surface->boundingRect());
    info.setIconGeometry(QRectF(surface->iconGeometry()));
    info.setPosition(surface->position());
    info.setVisible(surface->isVisible());
    info.setActive(surface->isActivated());
    info.setType(static_cast<int>(surface->type()));
    info.setState(static_cast<int>(surface->surfaceState()));

    QString outputName;
    if (auto *output = surface->ownsOutput(); output && output->output()) {
        outputName = output->output()->name();
    }
    info.setOutput(outputName);

    QString title;
    if (auto *shellSurface = surface->shellSurface()) {
        title = shellSurface->property("title").toString();
    }
    info.setTitle(title);
    return info;
}

void TreelandRemoteSource::collectSurfaceInfos(QList<WindowInfo> &infos,
                                               SurfaceWrapper *surface,
                                               int layer,
                                               const QString &containerName,
                                               int z) const
{
    infos.append(buildWindowInfo(surface, layer, containerName, z));
    const auto subSurfaces = surface->subSurfaces();
    for (auto *child : subSurfaces) {
        if (child) {
            collectSurfaceInfos(infos, child, layer, containerName, z);
        }
    }
}

void TreelandRemoteSource::collectWorkspaceModelWindows(QList<WindowInfo> &infos,
                                                        WorkspaceModel *workspaceModel,
                                                        int layer,
                                                        const QString &containerName) const
{
    if (!workspaceModel)
        return;

    const QList<SurfaceWrapper *> workspaceSurfaces = workspaceModel->surfaces();
    for (int index = 0; index < workspaceSurfaces.size(); ++index) {
        auto *surface = workspaceSurfaces.at(index);
        if (surface && !surface->parentSurface()) {
            collectSurfaceInfos(infos, surface, layer, containerName, index);
        }
    }
}

void TreelandRemoteSource::collectCurrentWorkspaceModelWindows(QList<WindowInfo> &infos,
                                                        WorkspaceModel *workspaceModel,
                                                        int layer,
                                                        const QString &containerName) const
{
    if (!workspaceModel)
        return;

    QList<SurfaceWrapper *> workspaceSurfaces;
    int currentId = workspaceModel->id();
    WOutputRenderWindow::paintOrderItemList(
        Helper::instance()->workspace(),
        [&workspaceSurfaces, currentId](QQuickItem *item) -> bool {
            auto surfaceWrapper = qobject_cast<SurfaceWrapper *>(item);
            if (surfaceWrapper && surfaceWrapper->showOnWorkspace(currentId)) {
                workspaceSurfaces.append(surfaceWrapper);
                return true;
            } else {
                return false;
            }
        });

    for (int index = 0; index < workspaceSurfaces.size(); ++index) {
        auto *surface = workspaceSurfaces.at(index);
        if (surface && !surface->parentSurface()) {
            collectSurfaceInfos(infos, surface, layer, containerName, index);
        }
    }
}

LayerInfo TreelandRemoteSource::buildLayerInfo(SurfaceContainer *container) const
{
    LayerInfo layerInfo;
    const int layer = static_cast<int>(container->z());
    const QString containerName = container->objectName();

    layerInfo.setName(containerName);
    layerInfo.setLayer(layer);

    if (auto *workspace = qobject_cast<Workspace *>(container)) {
        QList<WorkspaceInfo> workspaces;
        for (auto *workspaceModel : workspace->models()->objects()) {
            WorkspaceInfo workspaceInfo;
            workspaceInfo.setId(workspaceModel->id());
            workspaceInfo.setIsActive(workspaceModel == workspace->current());
            QList<WindowInfo> windows;
            if (workspaceInfo.isActive()) {
                collectCurrentWorkspaceModelWindows(windows, workspaceModel, layer, containerName);
            } else {
                collectWorkspaceModelWindows(windows, workspaceModel, layer, containerName);
            }
            workspaceInfo.setWindows(windows);
            workspaces.append(workspaceInfo);
        }
        layerInfo.setWorkspaces(workspaces);
    } else {
        QList<WindowInfo> windows;
        const QList<SurfaceWrapper *> surfaces = container->surfaces();
        for (int index = 0; index < surfaces.size(); ++index) {
            auto *surface = surfaces.at(index);
            if (surface && !surface->parentSurface()) {
                collectSurfaceInfos(windows, surface, layer, containerName, index);
            }
        }
        layerInfo.setWindows(windows);
    }

    return layerInfo;
}

void TreelandRemoteSource::updateCursor(const QPointF &newPosition)
{
    if (newPosition != m_cursorPosition) {
        m_cursorPosition = newPosition;
        Q_EMIT cursorPositionChanged(m_cursorPosition);
    }
}
