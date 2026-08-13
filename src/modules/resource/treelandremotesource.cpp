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
#include <winputdevice.h>
#include <woutput.h>
#include <woutputrenderwindow.h>
#include <wseat.h>
#include <wserver.h>
#include <wsurfaceitem.h>
#include <wtextureproviderprovider.h>

#include <wlr_all.h>
#include <wayland-server-core.h>

#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImage>
#include <QLocalServer>
#include <QMetaEnum>
#include <QRemoteObjectHost>
#include <QSet>
#include <QTimer>
#include <QUrl>

WAYLIB_SERVER_USE_NAMESPACE

namespace {

uint32_t currentTimeMs()
{
    return static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF);
}

QString processNameForPid(int pid)
{
    if (pid <= 0)
        return {};
    QFile comm(QStringLiteral("/proc/%1/comm").arg(pid));
    if (comm.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(comm.readLine()).trimmed();
    return {};
}

// Grabs a texture-backed item into a QImage, blocking the current thread
// (spinning a local event loop with a timeout) until the GPU read-back
// completes. Returns false on timeout or failure; never throws past the
// boundary.
bool grabToImage(WTextureProviderProvider *provider, QImage *out)
{
    if (!provider || !out)
        return false;

    WTextureCapturer capturer(provider);
    QFuture<QImage> future = capturer.grabToImage();
    QFutureWatcher<QImage> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<QImage>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(future);
    // Bound the wait so a stuck render pipeline cannot hang the compositor.
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    if (!watcher.isFinished())
        return false;
    try {
        *out = watcher.result();
    } catch (...) {
        return false;
    }
    return !out->isNull();
}

QString saveImage(const QImage &image, QString filePath)
{
    if (image.isNull())
        return {};
    if (filePath.isEmpty())
        filePath = QStringLiteral("/tmp/treeland-debug-%1.png").arg(QDateTime::currentMSecsSinceEpoch());
    if (!QFileInfo(filePath).suffix().isEmpty()) {
        // keep the user supplied suffix (must be a format QImage can write)
    } else {
        filePath += QStringLiteral(".png");
    }
    const QByteArray format = QFileInfo(filePath).suffix().toLower().toUtf8();
    if (!image.save(filePath, format.isEmpty() ? "png" : format.constData()))
        return {};
    return filePath;
}

} // namespace

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
    info.setId(reinterpret_cast<qint64>(surface));
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

void TreelandRemoteSource::collectAllToplevelSurfaces(QList<SurfaceWrapper *> &out) const
{
    auto *root = Helper::instance()->rootSurfaceContainer();
    if (!root)
        return;

    QSet<SurfaceWrapper *> seen;
    const auto appendTop = [&out, &seen](SurfaceWrapper *surface) {
        if (surface && !surface->parentSurface() && !seen.contains(surface)) {
            seen.insert(surface);
            out.append(surface);
        }
    };

    for (auto *container : root->subContainers()) {
        if (!container)
            continue;
        if (auto *workspace = qobject_cast<Workspace *>(container)) {
            if (auto *models = workspace->models()) {
                for (auto *model : models->objects()) {
                    if (!model)
                        continue;
                    for (auto *surface : model->surfaces())
                        appendTop(surface);
                }
            }
        } else {
            for (auto *surface : container->surfaces())
                appendTop(surface);
        }
    }
}

SurfaceWrapper *TreelandRemoteSource::findSurfaceById(qint64 id) const
{
    QList<SurfaceWrapper *> surfaces;
    collectAllToplevelSurfaces(surfaces);
    for (auto *surface : surfaces) {
        if (reinterpret_cast<qint64>(surface) == id)
            return surface;
    }
    return nullptr;
}

QList<WindowInfo> TreelandRemoteSource::getWindows()
{
    QList<SurfaceWrapper *> surfaces;
    collectAllToplevelSurfaces(surfaces);

    QList<WindowInfo> infos;
    infos.reserve(surfaces.size());
    for (auto *surface : surfaces) {
        auto *container = surface->container();
        const QString containerName = container ? container->objectName() : QString();
        const int layer = container ? static_cast<int>(container->z()) : 0;
        infos.append(buildWindowInfo(surface, layer, containerName, static_cast<int>(surface->z())));
    }
    return infos;
}

QList<ClientInfo> TreelandRemoteSource::getClients()
{
    QList<ClientInfo> result;
    auto *helper = Helper::instance();
    auto *server = helper ? helper->server() : nullptr;
    auto *display = server ? server->handle() : nullptr;

    struct ClientEntry {
        qint64 id = 0;
        int pid = 0;
        QString executable;
        QList<WindowInfo> windows;
    };
    QList<ClientEntry> entries;
    QHash<wl_client *, int> indexByClient;

    const auto ensureEntry = [&](wl_client *client) -> int {
        if (!client)
            return -1;
        auto it = indexByClient.constFind(client);
        if (it != indexByClient.constEnd())
            return it.value();
        ClientEntry entry;
        entry.id = reinterpret_cast<qint64>(client);
        pid_t pid = 0;
        uid_t uid = 0;
        gid_t gid = 0;
        wl_client_get_credentials(client, &pid, &uid, &gid);
        entry.pid = pid;
        entry.executable = processNameForPid(pid);
        indexByClient.insert(client, entries.size());
        entries.append(entry);
        return entries.size() - 1;
    };

    // Group toplevel windows by their owning Wayland client.
    QList<SurfaceWrapper *> surfaces;
    collectAllToplevelSurfaces(surfaces);
    for (auto *surface : surfaces) {
        auto *wsurface = surface->surface();
        if (!wsurface || !wsurface->handle())
            continue;
        wl_client *client = wl_resource_get_client(wsurface->handle()->resource);
        const int index = ensureEntry(client);
        if (index < 0)
            continue;
        auto *container = surface->container();
        const QString containerName = container ? container->objectName() : QString();
        const int layer = container ? static_cast<int>(container->z()) : 0;
        entries[index].windows.append(
            buildWindowInfo(surface, layer, containerName, static_cast<int>(surface->z())));
    }

    // Also include connected clients that own no toplevel window.
    if (display) {
        struct wl_list *clientList = wl_display_get_client_list(display);
        wl_client *client = nullptr;
        wl_client_for_each(client, clientList)
        {
            ensureEntry(client);
        }
    }

    for (const auto &entry : std::as_const(entries)) {
        ClientInfo info;
        info.setId(entry.id);
        info.setPid(entry.pid);
        info.setExecutable(entry.executable);
        info.setWindows(entry.windows);
        result.append(info);
    }
    return result;
}

bool TreelandRemoteSource::activateWindow(qint64 id)
{
    auto *surface = findSurfaceById(id);
    if (!surface)
        return false;
    Helper::instance()->forceActivateSurface(surface);
    return true;
}

bool TreelandRemoteSource::closeWindow(qint64 id)
{
    auto *surface = findSurfaceById(id);
    if (!surface)
        return false;
    surface->close();
    return true;
}

bool TreelandRemoteSource::minimizeWindow(qint64 id)
{
    auto *surface = findSurfaceById(id);
    if (!surface)
        return false;
    surface->minimize(false);
    return true;
}

bool TreelandRemoteSource::toggleMaximized(qint64 id)
{
    auto *surface = findSurfaceById(id);
    if (!surface)
        return false;
    surface->toggleMaximized();
    return true;
}

bool TreelandRemoteSource::toggleFullscreen(qint64 id)
{
    auto *surface = findSurfaceById(id);
    if (!surface)
        return false;
    if (surface->surfaceState() == SurfaceWrapper::State::Fullscreen)
        surface->leaveFullscreen();
    else
        surface->enterFullscreen();
    return true;
}

bool TreelandRemoteSource::moveWindow(qint64 id, int x, int y)
{
    auto *surface = findSurfaceById(id);
    if (!surface)
        return false;
    surface->moveNormalGeometryInOutput(QPointF(x, y));
    return true;
}

bool TreelandRemoteSource::resizeWindow(qint64 id, int w, int h)
{
    auto *surface = findSurfaceById(id);
    if (!surface || w <= 0 || h <= 0)
        return false;
    return surface->resize(QSizeF(w, h));
}

bool TreelandRemoteSource::setWindowWorkspace(qint64 id, int workspaceId)
{
    auto *surface = findSurfaceById(id);
    if (!surface)
        return false;
    surface->setWorkspaceId(workspaceId);
    return true;
}

bool TreelandRemoteSource::moveCursor(QPointF pos)
{
    auto *seat = Helper::instance()->seat();
    if (!seat)
        return false;
    seat->setCursorPosition(pos);
    return true;
}

bool TreelandRemoteSource::sendPointerButton(int button, bool pressed)
{
    auto *seat = Helper::instance()->seat();
    if (!seat || !seat->handle())
        return false;
    wlr_seat_pointer_notify_button(seat->handle(), currentTimeMs(),
                                   static_cast<uint32_t>(button),
                                   pressed ? WL_POINTER_BUTTON_STATE_PRESSED
                                           : WL_POINTER_BUTTON_STATE_RELEASED);
    wlr_seat_pointer_notify_frame(seat->handle());
    return true;
}

bool TreelandRemoteSource::sendKey(int keycode, bool pressed)
{
    auto *seat = Helper::instance()->seat();
    if (!seat || !seat->handle())
        return false;
    // Make sure the seat has a keyboard bound so the key event is delivered.
    if (auto *kbdDevice = seat->keyboardGroupKeyboard()) {
        if (kbdDevice->handle()
            && kbdDevice->handle()->type == WLR_INPUT_DEVICE_KEYBOARD) {
            if (auto *keyboard = wlr_keyboard_from_input_device(kbdDevice->handle()))
                wlr_seat_set_keyboard(seat->handle(), keyboard);
        }
    }
    wlr_seat_keyboard_notify_key(seat->handle(), currentTimeMs(),
                                 static_cast<uint32_t>(keycode),
                                 pressed ? 1u : 0u);
    return true;
}

QString TreelandRemoteSource::captureOutput(QString outputName, QString filePath)
{
    auto *root = Helper::instance()->rootSurfaceContainer();
    if (!root)
        return {};

    WOutputViewport *viewport = nullptr;
    if (outputName.isEmpty()) {
        // No name given: prefer the primary output, else the first available.
        if (auto *primary = root->primaryOutput())
            viewport = primary->screenViewport();
        if (!viewport) {
            for (auto *output : root->outputs()) {
                if (output && output->screenViewport()) {
                    viewport = output->screenViewport();
                    break;
                }
            }
        }
    } else {
        for (auto *output : root->outputs()) {
            if (!output || !output->screenViewport())
                continue;
            if (output->getOutputId() == outputName
                || (output->output() && output->output()->name() == outputName)) {
                viewport = output->screenViewport();
                break;
            }
        }
    }
    if (!viewport)
        return {};

    QImage image;
    if (!grabToImage(viewport, &image))
        return {};
    return saveImage(image, filePath);
}

QString TreelandRemoteSource::captureScreen(QString filePath)
{
    auto *root = Helper::instance()->rootSurfaceContainer();
    if (!root)
        return {};
    auto *primary = root->primaryOutput();
    if (!primary)
        return captureOutput({}, filePath);
    return captureOutput(primary->getOutputId(), filePath);
}

QString TreelandRemoteSource::captureWindow(qint64 id, QString filePath)
{
    auto *surface = findSurfaceById(id);
    if (!surface || !surface->surfaceItem())
        return {};
    auto *content = surface->surfaceItem()->findItemContent();
    if (!content)
        return {};
    QImage image;
    if (!grabToImage(content, &image))
        return {};
    return saveImage(image, filePath);
}
