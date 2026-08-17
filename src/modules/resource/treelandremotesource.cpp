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
#include <wsurface.h>
#include <wsurfaceitem.h>
#include <woutputrenderwindow.h>
#include <wseat.h>
#include <wserver.h>
#include <wsocket.h>
#include <wtextureproviderprovider.h>

#include <wlr_all.h>
#include <wayland-server-core.h>

#include <QBuffer>
#include <QDateTime>
#include <QEventLoop>
#include <QFile>
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
//
// NOTE: unlike capture.cpp's asynchronous WTextureCapturer usage, this spins a
// nested QEventLoop on the compositor main thread so the RPC can return the
// image synchronously. While loop.exec() runs the main thread keeps pumping
// events (render frames, other IPC calls, window management), which can
// re-enter this code path or mutate the scene graph. The 5s timeout bounds the
// worst case, and on the common path (not currently rendering) doGrabToImage
// runs synchronously and the loop exits immediately. This blocking behaviour
// is acceptable ONLY because the debug Remote Object source is opt-in
// (debugSource DConfig) and not present in normal operation.
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
    *out = watcher.result();
    return !out->isNull();
}

// Encodes a grabbed frame as PNG bytes. The compositor never writes to disk;
// the treeland-debug client owns the file. Returns an empty array on failure.
QByteArray encodeImage(const QImage &image)
{
    if (image.isNull())
        return {};
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly))
        return {};
    if (!image.save(&buffer, "png"))
        return {};
    return bytes;
}

// Stable window id: the wl_resource id of the surface's underlying wl_surface.
// This is the Wayland-protocol identity of the surface (rather than an
// internal C++ pointer) and is what treeland-debug exposes to the user.
// Note: wl_resource ids are unique per client, so two windows from different
// clients can in principle share an id; findSurfaceById() matches the first
// occurrence, which is acceptable for a debug tool whose output is grouped
// by client.
qint64 surfaceId(WSurface *ws)
{
    if (!ws || !ws->handle() || !ws->handle()->resource)
        return 0;
    return static_cast<qint64>(wl_resource_get_id(ws->handle()->resource));
}

} // namespace

TreelandRemoteSource::TreelandRemoteSource(QObject *parent)
    : WindowTreeRemoteSource(parent)
{
    auto *host = new QRemoteObjectHost(this);
    QRemoteObjectHost::setLocalServerOptions(QLocalServer::UserAccessOption);
    host->setHostUrl(QUrl(QStringLiteral("local:org.deepin.dde.treeland.debug")));
    host->enableRemoting(this, QStringLiteral("WindowTree"));
}

TreelandRemoteSource::~TreelandRemoteSource()
{
    stopEventCapture();
}

QPointF TreelandRemoteSource::cursorPosition() const
{
    // Read the live cursor position on demand. We deliberately do NOT keep a
    // WCursor::positionChanged connection alive between requests, so there is
    // zero per-pointer-motion overhead when no treeland-debug client is
    // connected. The READONLY property is only ever read on replica connect,
    // which is exactly when a client asked for it.
    if (auto *root = Helper::instance()->rootSurfaceContainer(); root && root->cursor())
        return root->cursor()->position();
    return {};
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
    info.setId(surfaceId(surface->surface()));
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

    // Frame/damage statistics from the underlying wlr_surface.
    if (auto *w = surface->surface()) {
        if (auto *wlr = w->handle()) {
            info.setFrames(static_cast<qint64>(wlr->current.seq));
            const auto *extents = pixman_region32_extents(&wlr->current.buffer_damage);
            if (extents)
                info.setDamage(QRectF(extents->x1, extents->y1,
                                      extents->x2 - extents->x1,
                                      extents->y2 - extents->y1));
        }
    }
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
        if (surfaceId(surface->surface()) == id)
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
        QString appId;
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
        // Use WClient's own credential/appId accessors instead of reaching
        // into libwayland directly.
        if (auto *wclient = WClient::get(client)) {
            if (auto creds = wclient->credentials())
                entry.pid = creds->pid;
            entry.appId = QString::fromUtf8(wclient->appId());
        } else if (auto creds = WClient::getCredentials(client)) {
            entry.pid = creds->pid;
        }
        entry.executable = processNameForPid(entry.pid);
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
        info.setAppId(entry.appId);
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
    auto *workspace = Helper::instance()->workspace();
    if (!workspace || !workspace->modelFromId(workspaceId))
        return false;
    // moveSurfaceTo() Q_ASSERTs that the surface already belongs to a workspace
    // model (workspaceId() != -1); layer-shell surfaces (workspaceId == -1)
    // live outside workspaces and must not be moved this way, so reject them
    // up front. ShowOnAllWorkspaceId (-2) is fully supported by moveSurfaceTo.
    if (surface->workspaceId() == -1)
        return false;
    workspace->moveSurfaceTo(surface, workspaceId);
    return true;
}

bool TreelandRemoteSource::moveCursor(QPointF pos)
{
    auto *helper = Helper::instance();
    if (!helper)
        return false;
    // Use the high-level Helper API: it ends any in-progress interactive
    // move/resize for every seat before moving the cursor, so injected motion
    // is never accidentally fed into an active move/resize transaction.
    helper->setCursorPosition(pos);
    return true;
}

bool TreelandRemoteSource::sendPointerButton(int button, bool pressed)
{
    auto *seat = Helper::instance()->seat();
    if (!seat || !seat->handle())
        return false;
    wlr_seat_pointer_notify_button(seat->handle(), QDateTime::currentMSecsSinceEpoch(),
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
    wlr_seat_keyboard_notify_key(seat->handle(), QDateTime::currentMSecsSinceEpoch(),
                                 static_cast<uint32_t>(keycode),
                                 pressed ? 1u : 0u);
    return true;
}

QByteArray TreelandRemoteSource::captureOutput(QString outputName)
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
    return encodeImage(image);
}

QByteArray TreelandRemoteSource::captureScreen()
{
    auto *root = Helper::instance()->rootSurfaceContainer();
    if (!root)
        return {};
    auto *primary = root->primaryOutput();
    if (!primary)
        return captureOutput({});
    return captureOutput(primary->getOutputId());
}

QByteArray TreelandRemoteSource::captureWindow(qint64 id)
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
    return encodeImage(image);
}

QList<DebugEvent> TreelandRemoteSource::getEvents(quint64 afterSeq)
{
    // A live poller keeps the event filter installed; if no poller calls
    // within the idle window the filter is removed and the ring buffer freed,
    // so there is zero per-event overhead when nobody is monitoring.
    startEventCapture();
    if (!m_eventIdleTimer) {
        m_eventIdleTimer = new QTimer(this);
        m_eventIdleTimer->setSingleShot(true);
        connect(m_eventIdleTimer, &QTimer::timeout, this, [this] { stopEventCapture(); });
    }
    constexpr int EventCaptureIdleMs = 5000;
    m_eventIdleTimer->start(EventCaptureIdleMs);

    QList<DebugEvent> result;
    for (const auto &e : m_events) {
        if (e.seq() > afterSeq)
            result.append(e);
    }
    return result;
}

qint64 TreelandRemoteSource::focusedWindowId()
{
    auto *seat = Helper::instance()->seat();
    if (!seat) return 0;
    return surfaceId(seat->keyboardFocusSurface());
}

qint64 TreelandRemoteSource::windowUnderCursor()
{
    auto *seat = Helper::instance()->seat();
    if (!seat) return 0;
    return surfaceId(seat->pointerFocusSurface());
}

void TreelandRemoteSource::startEventCapture()
{
    if (m_eventCaptureActive)
        return;
    auto *window = Helper::instance() ? Helper::instance()->window() : nullptr;
    if (!window)
        return;
    window->installEventFilter(this);
    m_eventCaptureActive = true;
}

void TreelandRemoteSource::stopEventCapture()
{
    if (!m_eventCaptureActive)
        return;
    if (auto *window = Helper::instance() ? Helper::instance()->window() : nullptr)
        window->removeEventFilter(this);
    m_eventCaptureActive = false;
    m_events.clear(); // release the ring buffer while idle
}

void TreelandRemoteSource::appendEvent(int type, qint64 target, const QString &detail)
{
    DebugEvent e;
    e.setSeq(m_nextEventSeq++);
    e.setType(type);
    e.setTarget(target);
    e.setDetail(detail);
    e.setTimestampMs(static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()));
    m_events.append(e);
    if (m_events.size() > MAX_EVENTS)
        m_events.removeFirst();
}

bool TreelandRemoteSource::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)
    // The filter is installed only while a treeland-debug client is actively
    // polling getEvents(). The target surface is read from the seat's current
    // focus, so on a focus transition it may reflect the previous event —
    // acceptable for a debug stream.
    auto *seat = Helper::instance() ? Helper::instance()->seat() : nullptr;
    switch (event->type()) {
    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent *>(event);
        appendEvent(1, surfaceId(seat ? seat->pointerFocusSurface() : nullptr),
                    QStringLiteral("motion %1,%2").arg(me->position().x()).arg(me->position().y()));
        break;
    }
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent *>(event);
        appendEvent(2, surfaceId(seat ? seat->pointerFocusSurface() : nullptr),
                    QStringLiteral("button %1 %2 @%3,%4")
                        .arg(me->button())
                        .arg(event->type() == QEvent::MouseButtonPress ? "press" : "release")
                        .arg(me->position().x()).arg(me->position().y()));
        break;
    }
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
        auto *ke = static_cast<QKeyEvent *>(event);
        appendEvent(3, surfaceId(seat ? seat->keyboardFocusSurface() : nullptr),
                    QStringLiteral("key %1 %2")
                        .arg(ke->key())
                        .arg(event->type() == QEvent::KeyPress ? "press" : "release"));
        break;
    }
    default:
        break;
    }
    return false; // never consume — only observe
}
