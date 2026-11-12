// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treelandremotesource.h"

#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "idle/idlemanager.h"
#include "modules/shortcut/shortcutcontroller.h"
#include "output/output.h"
#include "core/waylandserver.h"
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
#include <xkbcommon/xkbcommon.h>
#include <cstring>
#include <utility>

#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImage>
#include <QLocalServer>
#include <QMetaEnum>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QRemoteObjectHost>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "treelanddebugsocket.h"
#include "common/treelandlogging.h"
#include <private/qxkbcommon_p.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {

// A requested no-auth window closes when it went this long without a single
// call, and every call pushes it out again.
constexpr int NoAuthIdleTimeout = 30 * 60 * 1000;

QString executableForPid(int pid)
{
    if (pid <= 0)
        return {};
    // /proc/<pid>/exe is a symlink to the real executable path. Use
    // QFileInfo::symLinkTarget() so the read is cross-platform and returns
    // an empty string when the link is unreadable (e.g. another user's pid).
    return QFileInfo(QStringLiteral("/proc/%1/exe").arg(pid)).symLinkTarget();
}

QString commandForPid(int pid)
{
    if (pid <= 0)
        return {};
    // /proc/<pid>/cmdline is the full command line with arguments separated
    // by NUL bytes. Join them with spaces for a human/debug-friendly string.
    QFile cmdline(QStringLiteral("/proc/%1/cmdline").arg(pid));
    if (!cmdline.open(QIODevice::ReadOnly))
        return {};
    const QByteArray raw = cmdline.readAll();
    const QList<QByteArray> args = raw.split('\0');
    QStringList parts;
    for (const auto &arg : args) {
        if (!arg.isEmpty())
            parts << QString::fromUtf8(arg);
    }
    return parts.join(' ');
}

// Build the same covered area without assuming that Qt and pixman retain
// the same rectangle decomposition (as WTools::fromPixmanRegion does).
QRegion regionFromPixman(const pixman_region32_t *region)
{
    int count = 0;
    const pixman_box32_t *boxes = pixman_region32_rectangles(region, &count);
    QRegion result;
    for (int i = 0; i < count; ++i) {
        result += QRect(QPoint(boxes[i].x1, boxes[i].y1),
                        QPoint(boxes[i].x2 - 1, boxes[i].y2 - 1));
    }
    return result;
}

// Encodes a grabbed frame as PNG bytes. The server never writes to disk;
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

// Recursively searches a QQuickItem subtree for the first surface content
// item (a WTextureProviderProvider). Used as a fallback in captureWindow when
// a surface's own findItemContent() returns null (e.g. menus/popups whose
// content lives in a child item).
WSurfaceItemContent *findContentInSubtree(QQuickItem *item)
{
    if (!item)
        return nullptr;
    if (auto *content = qobject_cast<WSurfaceItemContent *>(item))
        return content;
    const auto children = item->childItems();
    for (auto *child : children) {
        if (auto *content = findContentInSubtree(child))
            return content;
    }
    return nullptr;
}


// Appends one line per QQuickItem in the subtree rooted at @p item, indented
// by depth, describing the item's type/objectName/geometry/visibility. Used
// by getSceneTree() to expose the live QtQuick scene graph to treeland-debug.
void dumpSceneTree(QQuickItem *item, int depth, QString &out)
{
    if (!item)
        return;
    const QString indent(depth * 2, ' ');
    QString line = indent + QString::fromUtf8(item->metaObject()->className());
    const QString name = item->objectName();
    if (!name.isEmpty())
        line += QStringLiteral(" \"%1\"").arg(name);
    line += QStringLiteral(" [%1,%2 %3x%4]")
                .arg(QString::number(item->x(), 'f', 1))
                .arg(QString::number(item->y(), 'f', 1))
                .arg(QString::number(item->width(), 'f', 1))
                .arg(QString::number(item->height(), 'f', 1));
    if (!item->isVisible())
        line += QStringLiteral(" hidden");
    line += QStringLiteral(" z=%1 op=%2")
                .arg(QString::number(item->z(), 'f', 1))
                .arg(QString::number(item->opacity(), 'f', 2));
    out += line + '\n';
    const auto children = item->childItems();
    for (auto *child : children)
        dumpSceneTree(child, depth + 1, out);
}

// Stable window id: the wl_resource pointer of the surface's underlying
// wl_surface. The wl_resource *id* is only unique per wl_client, so two
// windows from different clients can share an id — unsafe for write
// operations (close/move/...) that could target the wrong window. The
// pointer value is unique across the whole server for the lifetime of
// the surface, which is good enough for a single-machine debug tool.
qint64 surfaceId(WSurface *ws)
{
    if (!ws || !ws->handle() || !ws->handle()->resource)
        return 0;
    return reinterpret_cast<qint64>(ws->handle()->resource);
}

// Converts a Qt::Key value to a Linux evdev keycode using the keyboard's
// current xkb keymap. Returns 0 if no keycode produces this key.
// The evdev/keysym conversion lives on the server side (where wlr_seat needs
// the code) while the client talks in portable Qt::Key values.
int qtKeyToEvdev(int qtKey, wlr_keyboard *keyboard)
{
    if (!keyboard || !keyboard->xkb_state)
        return 0;

    // Walk the live keymap and return the first keycode whose level-0 symbol
    // Qt maps back to the requested Qt::Key value, via Qt's own
    // QXkbCommon::keysymToQtKey (the same table used by XWayland), so we never
    // need a hand-maintained Qt::Key name -> keysym alias table.
    xkb_keymap *keymap = xkb_state_get_keymap(keyboard->xkb_state);
    const xkb_keycode_t min_kc = xkb_keymap_min_keycode(keymap);
    const xkb_keycode_t max_kc = xkb_keymap_max_keycode(keymap);
    for (xkb_keycode_t kc = min_kc; kc <= max_kc; ++kc) {
        const xkb_keysym_t *syms = nullptr;
        const int count = xkb_keymap_key_get_syms_by_level(keymap, kc, 0, 0, &syms);
        for (int i = 0; i < count; ++i) {
            if (QXkbCommon::keysymToQtKey(syms[i], Qt::NoModifier, keyboard->xkb_state, kc) == qtKey)
                return kc;
        }
    }
    return 0;
}

} // namespace

TreelandRemoteSource::TreelandRemoteSource(QObject *parent)
    : WindowTreeRemoteSource(parent)
{
    // Pick a conflict-free socket path. Release instances bind in their own
    // directory; a Debug development instance uses its UID-private /tmp
    // directory. A concurrent instance takes a numeric suffix, selected with
    // a non-blocking advisory lock.
    const QString socketDir = treelandDebugSocketDir();
    if (socketDir.isEmpty()) {
#ifdef TREELAND_DEBUG_DEV_BUILD
        const QString override = qEnvironmentVariable("TREELAND_DEBUG_DIR");
        const QString expectedDir = override.isEmpty()
            ? QStringLiteral("/tmp/treeland-debug-%1")
                  .arg(static_cast<qulonglong>(::geteuid()))
            : override;
        qCWarning(lcTlDebug) << "No usable development debug socket directory"
                             << expectedDir << "; debug remote source disabled";
#else
        qCWarning(lcTlDebug) << "No usable debug socket directory;"
                                "debug remote source disabled";
#endif
        return;
    }
    const QString socketPath = treelandDebugPickFreeSocketName(
        m_debugSocketLock,
        socketDir + QLatin1Char('/') + treelandDebugDefaultSocketName());
    if (socketPath.isEmpty()) {
        // All 100 candidate names are already locked — extremely unlikely
        // for a debug channel, but don't bind an invalid URL; leave the debug
        // source unpublished.
        qCWarning(lcTlDebug) << "No free debug socket name available; "
                                "debug remote source disabled";
        return;
    }

    if (!bindHost(socketPath)) {
        m_debugSocketLock.release();
        return;
    }

    // Release no-auth mode is a temporary debugging window requested over the
    // debug protocol itself (WindowTreeRemote::requestNoAuth): its socket opens
    // to every local user, and a call-free half hour closes it again.
    m_debugSocketPath = socketPath;
    m_noAuthIdleTimer = new QTimer(this);
    m_noAuthIdleTimer->setSingleShot(true);
    m_noAuthIdleTimer->setInterval(NoAuthIdleTimeout);
    connect(m_noAuthIdleTimer, &QTimer::timeout,
            this, &TreelandRemoteSource::closeNoAuthWindow);

    // Seal the socket here rather than trusting what startup found: a crash
    // during a no-auth window leaves the socket and directory open with no
    // record of it, so the state is written, not restored.
    closeNoAuthWindow();
#ifdef TREELAND_DEBUG_DEV_BUILD
    // Same-UID access is sufficient for local development and needs no polkit.
    // Keep the dev socket owner-only; the global Release no-auth policy is
    // separate and remains explicitly requested over the debug protocol.
    if (!treelandDebugSetSocketAccess(m_debugSocketPath, true)) {
        qCWarning(lcTlDebug) << "Cannot set owner-only access on the debug socket:"
                             << m_debugSocketPath << ':' << qt_error_string();
    }
#endif

    m_published = true;
}

TreelandRemoteSource::~TreelandRemoteSource()
{
    stopEventCapture();
    // The window may still be open (remoteDebug turned off, or shutdown): seal it
    // now instead of leaving it to the timer.  No rebinding here -- the host is
    // destroyed below, which drops every peer anyway.
    if (m_noAuthIdleTimer)
        m_noAuthIdleTimer->stop();
    if (m_socketRootOnly && !treelandDebugSetSocketAccess(m_debugSocketPath, false))
        qCWarning(lcTlDebug) << "Cannot seal the debug socket again:" << m_debugSocketPath
                             << ':' << qt_error_string();
    // Destroy the host (parent-owned child) before the socket lock member
    // releases, so the socket stops listening while we still hold the lock.
    // Otherwise a concurrent instance could acquire the lock and remove the
    // socket path while the old host is still alive.
    delete findChild<QRemoteObjectHost *>();
}

bool TreelandRemoteSource::bindHost(const QString &socketPath)
{
    auto *host = new QRemoteObjectHost(this);
    // Either way the host must bind the socket itself: enableRemoting() is a
    // no-op until the host has a hostUrl, so an externally managed
    // QLocalServer + addHostSideConnection() advertises nothing (#WM-693).
#ifdef TREELAND_DEBUG_DEV_BUILD
    // Debug builds: allow all users to connect (requirement: debug builds
    // are unrestricted).  WorldAccessOption sets the socket to 0666.
    QRemoteObjectHost::setLocalServerOptions(QLocalServer::WorldAccessOption);
    if (!host->setHostUrl(QUrl(QStringLiteral("local:%1").arg(socketPath)))) {
        qCWarning(lcTlDebug) << "Failed to listen on debug socket" << socketPath;
        delete host;
        return false;
    }
#else
    // Release builds: only root may connect.  The host binds with
    // UserAccessOption (0600, owned by dde) and the socket is then chmod'd to
    // 0000: connect() checks the socket inode's write permission, so dde and
    // every other non-root user get EACCES while root passes via
    // CAP_DAC_OVERRIDE.  Same policy as the previous SO_PEERCRED server, but
    // with the socket owned by the QtRO host, which enableRemoting() needs.
    QRemoteObjectHost::setLocalServerOptions(QLocalServer::UserAccessOption);
    if (!host->setHostUrl(QUrl(QStringLiteral("local:%1").arg(socketPath)))) {
        qCWarning(lcTlDebug) << "Failed to listen on debug socket" << socketPath;
        delete host;
        return false;
    }
    if (::chmod(QFile::encodeName(socketPath).constData(), 0) < 0) {
        qCWarning(lcTlDebug) << "Failed to restrict debug socket" << socketPath << ':'
                             << qt_error_string();
        delete host; // unlinks the socket file, so no reachable-but-deaf socket stays
        return false;
    }
    m_socketRootOnly = true;
#endif
    if (!host->enableRemoting(this, QStringLiteral("WindowTree"))) {
        qCWarning(lcTlDebug) << "Failed to enable remoting for WindowTree on socket"
                             << socketPath;
        delete host;
        return false;
    }
    return true;
}

bool TreelandRemoteSource::requestNoAuth()
{
    // A debug build opens the directory and the socket at startup, so there is
    // nothing left to open -- but the caller asked for the window, and it is
    // there.
    if (!m_socketRootOnly)
        return true;

    if (!treelandDebugSetSocketAccess(m_debugSocketPath, true)) {
        // Only a directory of Treeland's own can be widened, so a session
        // instance lands here and keeps requiring root.
        qCWarning(lcTlDebug) << "Cannot open the debug socket to every user:"
                             << m_debugSocketPath << ':' << qt_error_string()
                             << "(only the global service's own debug directory can be opened)";
        return false;
    }
    m_noAuthWindowOpen = true;
    m_noAuthIdleTimer->start();
    return true;
}

void TreelandRemoteSource::closeNoAuthWindow()
{
    // The constructor creates the timer last, so every early return (no runtime
    // directory, no free socket name, bind/chmod/enableRemoting failure) leaves
    // it null -- and such a source is deleted by WaylandServer to be retried.
    if (!m_noAuthIdleTimer)
        return;
    m_noAuthIdleTimer->stop();
    if (!m_socketRootOnly)
        return; // a debug build opens it at startup, for good; there is no window

    const bool windowWasOpen = std::exchange(m_noAuthWindowOpen, false);
    if (windowWasOpen) {
        // chmod 0000 only refuses *new* connects: a peer that got in during the
        // window keeps its socket and could stay privileged forever.  Rebuilding
        // the host closes every connection that was opened while it was open; the
        // seal below then closes the fresh socket and the directory again.
        if (auto *host = findChild<QRemoteObjectHost *>()) {
            delete host;
            if (!bindHost(m_debugSocketPath))
                qCWarning(lcTlDebug) << "Cannot rebind the debug socket" << m_debugSocketPath
                                     << "; the debug channel stays closed until the next start";
        }
    }

    if (!treelandDebugSetSocketAccess(m_debugSocketPath, false)) {
        qCWarning(lcTlDebug) << "Cannot seal the debug socket again:" << m_debugSocketPath
                             << ':' << qt_error_string();
    }
}

void TreelandRemoteSource::noteActivity() const
{
    // Only a call-free stretch closes the window: every call restarts it, and
    // the timer runs only while the window is open.
    if (m_noAuthIdleTimer && m_noAuthIdleTimer->isActive())
        m_noAuthIdleTimer->start();
}

QPointF TreelandRemoteSource::cursorPosition() const
{
    // The replica fetches this property when it connects, which is a request on
    // the debug socket like any other (treeland-debug cursor opens a connection
    // for every invocation), so it keeps the no-auth window alive too.
    noteActivity();
    // Read the live cursor position on demand. We deliberately do NOT keep a
    // WCursor::positionChanged connection alive between requests, so there is
    // zero per-pointer-motion overhead when no treeland-debug client is
    // connected. The READONLY property is only ever read on replica connect,
    // which is exactly when a client asked for it.
    if (auto *root = WaylandServer::instance()->rootSurfaceContainer(); root && root->cursor())
        return root->cursor()->position();
    return {};
}

TreelandInfo TreelandRemoteSource::getTreelandInfo()
{
    noteActivity();
    TreelandInfo info;

    auto mode = WaylandServer::instance()->currentMode();
    info.setCurrentMode(QString::fromUtf8(QMetaEnum::fromType<WaylandServer::CurrentMode>().valueToKey(static_cast<int>(mode))));

    if (auto *root = WaylandServer::instance()->rootSurfaceContainer()) {
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

// The rep field stays a string: treeland-debug prints it and a name reads better
// than a number in the JSON. This is the only place the enum becomes text, and
// the names below are the wire contract: camelCase on purpose, so
// QMetaEnum::valueToKey() (which would answer GeometryChanged) cannot be used.
static QString outputsUpdateReasonName(SurfaceWrapper::OutputsUpdateReason reason)
{
    switch (reason) {
    case SurfaceWrapper::OutputsUpdateReason::GeometryChanged:
        return QStringLiteral("geometryChanged");
    case SurfaceWrapper::OutputsUpdateReason::SurfaceAdded:
        return QStringLiteral("surfaceAdded");
    case SurfaceWrapper::OutputsUpdateReason::AddOutput:
        return QStringLiteral("addOutput");
    case SurfaceWrapper::OutputsUpdateReason::EnabledChanged:
        return QStringLiteral("enabledChanged");
    case SurfaceWrapper::OutputsUpdateReason::Unknown:
        break;
    }
    return {};
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
    info.setMinimized(surface->isMinimized());
    info.setType(static_cast<int>(surface->type()));
    info.setState(static_cast<int>(surface->surfaceState()));

    QString outputName;
    if (auto *output = surface->ownsOutput(); output && output->output()) {
        outputName = output->output()->name();
    }
    info.setOutput(outputName);

    // The outputs the window is on, and the one that drives its frame
    // callbacks.  A window can span several outputs, so the single owner name
    // above is not enough to tell where it is drawn.  Take the wrapper's list,
    // not the wl_surface's: a splash window has no entered outputs yet.
    QStringList outputNames;
    for (auto *output : surface->outputs()) {
        if (output)
            outputNames.append(output->name());
    }
    info.setOutputs(outputNames);

    QString framePacingOutputName;
    if (auto *w = surface->surface()) {
        if (auto *pacingOutput = w->framePacingOutput())
            framePacingOutputName = pacingOutput->name();
    }
    info.setFramePacingOutput(framePacingOutputName);

    const auto &diag = surface->outputsDiagnostics();
    info.setLastLeaveOutput(diag.lastLeaveOutput);
    info.setLastLeaveOutputEnabled(diag.lastLeaveOutputEnabled);
    info.setLastLeaveEmptiedOutputs(diag.lastLeaveEmptiedOutputs);
    info.setLastOutputsUpdateReason(outputsUpdateReasonName(diag.updateReason));
    info.setLastOutputsUpdateEnabled(diag.updateEnabledOutputs);
    info.setLastOutputsUpdateTotal(diag.updateTotalOutputs);
    info.setLastOutputsUpdateComputed(diag.updateComputedOutputs);
    info.setLastOutputsUpdateAutomatic(diag.updateAutomatic);

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
            // wlroots already clipped both regions to the surface size.
            info.setInputRegion(regionFromPixman(&wlr->input_region));
            info.setOpaqueRegion(regionFromPixman(&wlr->opaque_region));
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
        WaylandServer::instance()->workspace(),
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
    auto *root = WaylandServer::instance()->rootSurfaceContainer();
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
    noteActivity();
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

namespace {

// Shortcut names come from Wayland clients; keep them from forging extra lines
// or terminal escapes in the debug stream. Covers C0 (incl. \n and ESC), DEL
// and C1, which some terminals still interpret as escape introducers.
QString sanitizedName(const QString &name)
{
    QString safe = name;
    for (auto &ch : safe) {
        const ushort code = ch.unicode();
        if (code < 0x20 || (code >= 0x7f && code <= 0x9f))
            ch = QLatin1Char('?');
    }
    return safe;
}

QString shortcutFlagsText(ShortcutController::KeyFlags flags)
{
    QStringList parts;
    if (flags.testFlag(ShortcutController::KeyPress))
        parts << QStringLiteral("press");
    if (flags.testFlag(ShortcutController::KeyRelease))
        parts << QStringLiteral("release");
    if (flags.testFlag(ShortcutController::Repeat))
        parts << QStringLiteral("repeat");
    if (parts.isEmpty())
        parts << QStringLiteral("none");
    return parts.join('|');
}

QString swipeDirectionName(SwipeGesture::Direction direction)
{
    switch (direction) {
    case SwipeGesture::Direction::Down:
        return QStringLiteral("down");
    case SwipeGesture::Direction::Left:
        return QStringLiteral("left");
    case SwipeGesture::Direction::Up:
        return QStringLiteral("up");
    case SwipeGesture::Direction::Right:
        return QStringLiteral("right");
    default:
        return {};
    }
}

// One-line description of a bound shortcut, shared by the `shortcuts` listing
// and the registration event stream.
QString shortcutDetail(const ShortcutController::Binding &binding)
{
    const QString action = QString::fromUtf8(ShortcutController::actionName(binding.action));
    switch (binding.type) {
    case ShortcutController::Binding::Key:
        return QStringLiteral("\"%1\" = %2 -> %3 [%4]")
            .arg(sanitizedName(binding.name),
                 binding.key,
                 action,
                 shortcutFlagsText(binding.flags));
    // Every placeholder is filled in one arg() pass: a client-provided name
    // containing "%2" must not be substituted by the next chained arg().
    case ShortcutController::Binding::Swipe:
        return QStringLiteral("\"%1\" = swipe %2-finger %3 -> %4")
            .arg(sanitizedName(binding.name),
                 QString::number(binding.fingers),
                 swipeDirectionName(binding.direction),
                 action);
    case ShortcutController::Binding::Hold:
        return QStringLiteral("\"%1\" = hold %2-finger -> %3")
            .arg(sanitizedName(binding.name), QString::number(binding.fingers), action);
    }
    return {};
}

} // namespace

QList<ShortcutInfo> TreelandRemoteSource::getShortcuts()
{
    QList<ShortcutInfo> result;
    auto *server = WaylandServer::instance();
    auto *controller = server ? server->shortcutController() : nullptr;
    if (!controller)
        return result;

    const auto bindings = controller->bindings();
    result.reserve(bindings.size());
    for (const auto &binding : bindings) {
        ShortcutInfo info;
        info.setName(sanitizedName(binding.name));
        info.setType(binding.type);
        info.setKey(binding.key);
        info.setFlags(binding.flags);
        info.setAction(QString::fromUtf8(ShortcutController::actionName(binding.action)));
        info.setFingers(binding.fingers);
        info.setDirection(swipeDirectionName(binding.direction));
        result.append(info);
    }
    return result;
}

QList<ClientInfo> TreelandRemoteSource::getClients()
{
    noteActivity();
    QList<ClientInfo> result;
    auto *server = WaylandServer::instance();
    auto *nativeServer = server ? server->server() : nullptr;
    auto *display = nativeServer ? nativeServer->handle() : nullptr;

    struct ClientEntry {
        qint64 id = 0;
        QString appId;
        int pid = 0;
        QString executable;
        QString command;
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
        entry.executable = executableForPid(entry.pid);
        entry.command = commandForPid(entry.pid);
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
        info.setCommand(entry.command);
        info.setWindows(entry.windows);
        result.append(info);
    }
    return result;
}

IdleInfo TreelandRemoteSource::getIdleInfo()
{
    noteActivity();
    IdleInfo info;
    auto *server = WaylandServer::instance();
    auto *idle = server ? server->idleManager() : nullptr;
    if (!idle)
        return info;

    info.setInhibited(idle->isIdleInhibited());
    info.setScreensaverInhibited(idle->isScreensaverInhibited());
    info.setIdleMs(static_cast<quint64>(qMax<qint64>(idle->idleMs(), 0)));

    QList<IdleInhibitor> inhibitors;
    for (const auto &source : idle->inhibitors()) {
        IdleInhibitor entry;
        // Same id scheme as the window list, so an inhibitor surface shows up
        // in ``windows``/``top`` and can be used as a control target — but only
        // when the inhibitor is held on a toplevel surface. Layer-shell and
        // subsurface inhibitors (common for video players) report the raw
        // surface id, which the window list does not contain.
        entry.setSurfaceId(source.surface ? reinterpret_cast<qint64>(source.surface->resource) : 0);
        entry.setKind(source.kind);
        entry.setAppId(source.appId);
        entry.setTitle(source.title);
        entry.setPid(source.pid);
        entry.setExecutable(executableForPid(source.pid));
        entry.setCommand(commandForPid(source.pid));
        entry.setAppName(source.appName);
        entry.setReason(source.reason);
        entry.setActive(source.active);
        entry.setHeldMs(source.heldMs);
        inhibitors.append(entry);
    }
    info.setInhibitors(inhibitors);

    return info;
}

bool TreelandRemoteSource::activateWindow(qint64 id)
{
    noteActivity();
    auto *surface = findSurfaceById(id);
    if (!surface)
        return false;
    WaylandServer::instance()->forceActivateSurface(surface);
    return true;
}

bool TreelandRemoteSource::closeWindow(qint64 id)
{
    noteActivity();
    auto *surface = findSurfaceById(id);
    if (!surface)
        return false;
    surface->close();
    return true;
}

bool TreelandRemoteSource::minimizeWindow(qint64 id)
{
    noteActivity();
    auto *surface = findSurfaceById(id);
    if (!surface)
        return false;
    surface->minimize(false);
    return true;
}

bool TreelandRemoteSource::toggleMaximized(qint64 id)
{
    noteActivity();
    auto *surface = findSurfaceById(id);
    if (!surface)
        return false;
    surface->toggleMaximized();
    return true;
}

bool TreelandRemoteSource::toggleFullscreen(qint64 id)
{
    noteActivity();
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
    noteActivity();
    auto *surface = findSurfaceById(id);
    if (!surface)
        return false;
    surface->moveNormalGeometryInOutput(QPointF(x, y));
    return true;
}

bool TreelandRemoteSource::resizeWindow(qint64 id, int w, int h)
{
    noteActivity();
    auto *surface = findSurfaceById(id);
    if (!surface || w <= 0 || h <= 0)
        return false;
    return surface->resize(QSizeF(w, h));
}

bool TreelandRemoteSource::setWindowWorkspace(qint64 id, int workspaceId)
{
    noteActivity();
    auto *surface = findSurfaceById(id);
    if (!surface)
        return false;
    auto *workspace = WaylandServer::instance()->workspace();
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
    noteActivity();
    auto *server = WaylandServer::instance();
    if (!server)
        return false;
    // Use the high-level WaylandServer API: it ends any in-progress interactive
    // move/resize for every seat before moving the cursor, so injected motion
    // is never accidentally fed into an active move/resize transaction.
    server->setCursorPosition(pos);
    return true;
}

bool TreelandRemoteSource::sendPointerButton(int button, bool pressed)
{
    noteActivity();
    auto *seat = WaylandServer::instance()->seat();
    if (!seat || !seat->handle())
        return false;
    wlr_seat_pointer_notify_button(seat->handle(), QDateTime::currentMSecsSinceEpoch(),
                                   static_cast<uint32_t>(button),
                                   pressed ? WL_POINTER_BUTTON_STATE_PRESSED
                                           : WL_POINTER_BUTTON_STATE_RELEASED);
    wlr_seat_pointer_notify_frame(seat->handle());
    return true;
}

bool TreelandRemoteSource::sendKey(int qtKey, bool pressed)
{
    noteActivity();
    auto *seat = WaylandServer::instance()->seat();
    if (!seat || !seat->handle())
        return false;
    // Make sure the seat has a keyboard bound so the key event is delivered.
    wlr_keyboard *keyboard = nullptr;
    if (auto *kbdDevice = seat->keyboardGroupKeyboard()) {
        if (kbdDevice->handle()
            && kbdDevice->handle()->type == WLR_INPUT_DEVICE_KEYBOARD) {
            keyboard = wlr_keyboard_from_input_device(kbdDevice->handle());
            if (keyboard)
                wlr_seat_set_keyboard(seat->handle(), keyboard);
        }
    }
    // Convert the portable Qt::Key value to a Linux evdev keycode using the
    // keyboard's current xkb keymap. The client sends Qt::Key values (resolved
    // from names like "Escape" via QMetaEnum); the evdev conversion lives here
    // on the server side where wlr_seat expects evdev codes.
    const int evdevCode = qtKeyToEvdev(qtKey, keyboard);
    if (evdevCode == 0)
        return false;
    wlr_seat_keyboard_notify_key(seat->handle(), QDateTime::currentMSecsSinceEpoch(),
                                 static_cast<uint32_t>(evdevCode),
                                 pressed ? 1u : 0u);
    return true;
}

void TreelandRemoteSource::captureOutput(QString outputName)
{
    noteActivity();
    auto *root = WaylandServer::instance()->rootSurfaceContainer();
    if (!root) {
        Q_EMIT captureResult({});
        return;
    }

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
    if (!viewport) {
        Q_EMIT captureResult({});
        return;
    }

    // Async texture grab: start the GPU read-back and return immediately.
    // The server main thread is never blocked; the encoded PNG bytes are
    // delivered via the captureResult signal when the future completes.
    auto *capturer = new WTextureCapturer(viewport);
    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, capturer]() {
        const QByteArray bytes = encodeImage(watcher->result());
        capturer->deleteLater();
        watcher->deleteLater();
        Q_EMIT captureResult(bytes);
    });
    watcher->setFuture(capturer->grabToImage());
}

void TreelandRemoteSource::captureWindow(qint64 id)
{
    noteActivity();
    auto *surface = findSurfaceById(id);
    if (!surface || !surface->surfaceItem()) {
        Q_EMIT captureResult({});
        return;
    }
    auto *surfaceItem = surface->surfaceItem();
    // Prefer the surface's own texture-backed content. Some surfaces
    // (menus/popups) do not expose a content item via findItemContent(), so
    // search the item subtree for any WSurfaceItemContent and grab that.
    WSurfaceItemContent *content = surfaceItem->findItemContent();
    if (!content)
        content = findContentInSubtree(surfaceItem);

    if (content) {
        // Async texture grab — same pattern as captureOutput.
        auto *capturer = new WTextureCapturer(content);
        auto *watcher = new QFutureWatcher<QImage>(this);
        connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, capturer]() {
            const QByteArray bytes = encodeImage(watcher->result());
            capturer->deleteLater();
            watcher->deleteLater();
            Q_EMIT captureResult(bytes);
        });
        watcher->setFuture(capturer->grabToImage());
    } else {
        // Fallback: ask QtQuick to rasterise the whole item subtree into an
        // offscreen texture. This is also async — the ready signal fires
        // when the render is done. The QSharedPointer keeps the grab result
        // alive until the lambda completes.
        QSharedPointer<QQuickItemGrabResult> grab = surfaceItem->grabToImage();
        if (!grab) {
            Q_EMIT captureResult({});
            return;
        }
        connect(grab.data(), &QQuickItemGrabResult::ready, this, [this, grab]() {
            Q_EMIT captureResult(encodeImage(grab->image()));
        });
    }
}

QList<DebugEvent> TreelandRemoteSource::getEvents(quint64 afterSeq)
{
    noteActivity();
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
    noteActivity();
    auto *seat = WaylandServer::instance()->seat();
    if (!seat) return 0;
    return surfaceId(seat->keyboardFocusSurface());
}

qint64 TreelandRemoteSource::windowUnderCursor()
{
    noteActivity();
    auto *seat = WaylandServer::instance()->seat();
    if (!seat) return 0;
    return surfaceId(seat->pointerFocusSurface());
}

QString TreelandRemoteSource::getSceneTree(qint64 id)
{
    noteActivity();
    QQuickItem *root = nullptr;
    if (id != 0) {
        auto *surface = findSurfaceById(id);
        if (!surface)
            return QStringLiteral("No window with id %1.\n").arg(id);
        root = surface->surfaceItem();
        if (!root)
            return QStringLiteral("Window %1 has no scene item.\n").arg(id);
    } else {
        auto *window = WaylandServer::instance() ? WaylandServer::instance()->window() : nullptr;
        if (!window)
            return QStringLiteral("No render window available.\n");
        root = window->contentItem();
        if (!root)
            return QStringLiteral("Render window has no content item.\n");
    }
    QString out;
    dumpSceneTree(root, 0, out);
    if (out.isEmpty())
        return QStringLiteral("(empty scene)\n");
    return out;
}

void TreelandRemoteSource::startEventCapture()
{
    if (m_eventCaptureActive)
        return;
    auto *window = WaylandServer::instance() ? WaylandServer::instance()->window() : nullptr;
    if (!window)
        return;
    window->installEventFilter(this);
    m_eventCaptureActive = true;

    // Shortcut registrations and triggers are taken from the controller's
    // signals: a bound shortcut may swallow the key before it ever reaches the
    // render window, so the input filter would miss it.
    m_shortcutController = WaylandServer::instance()->shortcutController();
    if (!m_shortcutController)
        return;
    connect(m_shortcutController,
            &ShortcutController::shortcutRegistered,
            this,
            &TreelandRemoteSource::onShortcutRegistered);
    connect(m_shortcutController,
            &ShortcutController::shortcutUnregistered,
            this,
            &TreelandRemoteSource::onShortcutUnregistered);
    connect(m_shortcutController,
            &ShortcutController::actionTriggered,
            this,
            &TreelandRemoteSource::onShortcutTriggered);
    connect(m_shortcutController,
            &ShortcutController::actionFinished,
            this,
            &TreelandRemoteSource::onShortcutFinished);
    // Guarded: the controller outlives this source when remoteDebug is toggled,
    // and a provider left installed must not touch a destroyed source.
    const QPointer<TreelandRemoteSource> guardedThis(this);
    m_shortcutController->setTriggerTargetProvider(
        [guardedThis] { return guardedThis ? guardedThis->focusedWindowId() : 0; });
}

void TreelandRemoteSource::onShortcutRegistered(const ShortcutController::Binding &binding)
{
    appendEvent(6, 0, QStringLiteral("shortcut registered: ") + shortcutDetail(binding));
}

void TreelandRemoteSource::onShortcutUnregistered(const QString &name)
{
    appendEvent(7, 0, QStringLiteral("shortcut unregistered: \"%1\"").arg(sanitizedName(name)));
}

void TreelandRemoteSource::onShortcutTriggered(ShortcutAction action,
                                               const QString &name,
                                               bool isGesture,
                                               ShortcutController::KeyFlags keyFlags)
{
    // Gestures have no keyboard-focus target (a swipe while the lockscreen is
    // up would otherwise blame an unrelated window), so they report 0. For keys
    // the target was sampled before the action ran: a workspace/task switch
    // would otherwise report the destination window instead of the origin.
    const qint64 target = isGesture || !m_shortcutController
        ? 0
        : m_shortcutController->lastTriggerTarget();
    const QString trigger = isGesture
        ? QStringLiteral(" (gesture)")
        : QStringLiteral(" [%1]").arg(shortcutFlagsText(keyFlags));
    appendEvent(8,
                target,
                QStringLiteral("shortcut triggered: \"%1\" -> %2%3")
                    .arg(sanitizedName(name),
                         QString::fromUtf8(ShortcutController::actionName(action)),
                         trigger));
}

void TreelandRemoteSource::onShortcutFinished(ShortcutAction action,
                                              const QString &name,
                                              bool isTriggered)
{
    // Only swipe gestures emit actionFinished(); key and hold triggers arrive
    // through actionTriggered(), so there is no double counting.
    if (!isTriggered)
        return;
    appendEvent(8,
                0,
                QStringLiteral("shortcut triggered: \"%1\" -> %2 (gesture)")
                    .arg(sanitizedName(name),
                         QString::fromUtf8(ShortcutController::actionName(action))));
}

void TreelandRemoteSource::stopEventCapture()
{
    if (!m_eventCaptureActive)
        return;
    if (auto *window = WaylandServer::instance() ? WaylandServer::instance()->window() : nullptr)
        window->removeEventFilter(this);
    if (m_shortcutController) {
        m_shortcutController->setTriggerTargetProvider({});
        disconnect(m_shortcutController, nullptr, this, nullptr);
        m_shortcutController = nullptr;
    }
    m_eventCaptureActive = false;
    m_events.clear(); // release the ring buffer while idle
}

void TreelandRemoteSource::appendEvent(int type, qint64 target, const QString &detail)
{
    if (!m_eventCaptureActive)
        return;
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
    auto *seat = WaylandServer::instance() ? WaylandServer::instance()->seat() : nullptr;
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
