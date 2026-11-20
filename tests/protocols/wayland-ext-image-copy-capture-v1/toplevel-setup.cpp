// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wayland-ext-toplevel-image-capture-v1.h"
#include "server-bridge.h"
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "core/waylandserver.h"
#include "output/output.h"
#include "surface/surfacewrapper.h"
#include "surface/surfacecontainer.h"

#include <wbackend.h>
#include <wcursor.h>
#include <winputdevice.h>
#include <wseat.h>
#include <wbufferrenderer_p.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wsgtextureprovider.h>
#include <wsurfaceitem.h>
#include <wpointer.h>
#include <wlr_all.h>

#include <drm_fourcc.h>

#include <QImage>
#include <QEventLoop>
#include <QPointer>
#include <QQmlComponent>
#include <QSGNode>
#include <QSGRenderNode>
#include <private/qquickitem_p.h>

#include <cstdio>

#ifdef TREELAND_CAPTURE_TEST_GPU
#include <QDir>
#include <fcntl.h>
#include <unistd.h>
#endif

WAYLIB_SERVER_USE_NAMESPACE

namespace {
QPointer<SurfaceWrapper> g_target;
QPointer<SurfaceWrapper> g_occluder;
QImage g_screen;
QPointF g_position, g_surfacePosition, g_scenePosition;
QSizeF g_size, g_surfaceSize;
QQuickItem *g_parent = nullptr;
QQuickItem *g_surfaceParent = nullptr;
qreal g_scale = 0, g_surfaceScale = 0;
bool g_visible = false, g_surfaceVisible = false;
bool g_tracking = false, g_screenUnchanged = true, g_renderStateUnchanged = true;
int g_captureFramesChecked = 0;
wlr_pointer *g_pointer = nullptr;
uint32_t g_inputTime = 0;

QHash<QSGNode *, QMatrix4x4> nodeMatrices(QSGNode *root)
{
    QHash<QSGNode *, QMatrix4x4> matrices;
    if (!root)
        return matrices;
    switch (root->type()) {
    case QSGNode::TransformNodeType:
        matrices.insert(root, static_cast<QSGTransformNode *>(root)->combinedMatrix());
        break;
    case QSGNode::GeometryNodeType:
    case QSGNode::ClipNodeType: {
        auto *node = static_cast<QSGBasicGeometryNode *>(root);
        matrices.insert(root, node->matrix() ? *node->matrix() : QMatrix4x4());
        break;
    }
    case QSGNode::RenderNodeType: {
        auto *node = static_cast<QSGRenderNode *>(root);
        matrices.insert(root, node->matrix() ? *node->matrix() : QMatrix4x4());
        break;
    }
    default:
        break;
    }
    for (auto *child = root->firstChild(); child; child = child->nextSibling())
        matrices.insert(nodeMatrices(child));
    return matrices;
}

WOutputViewport *screenViewport()
{
    return WaylandServer::instance()->rootSurfaceContainer()->outputs().first()->screenViewport();
}

QImage readScreen()
{
    auto *viewport = screenViewport();
    auto *provider = viewport->wTextureProvider();
    auto *buffer = provider ? provider->wlrBuffer() : nullptr;
    if (!buffer)
        return {};
    WBufferUnlockPtr guard(wlr_buffer_lock(buffer));
    WUniquePointer<wlr_texture> texture(wlr_texture_from_buffer(viewport->output()->renderer(), buffer));
    if (!texture)
        return {};
    // Use the same explicit format as the capture client. A renderer's
    // preferred format (e.g. XBGR8888) need not have a WTools/QImage mapping;
    // passing a null image to glReadPixels would crash before capture starts.
    QImage image(buffer->width, buffer->height, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        qFatal("Could not allocate capture test output readback image");
    const wlr_texture_read_pixels_options options {
        .data = image.bits(),
        .format = DRM_FORMAT_ARGB8888,
        .stride = uint32_t(image.bytesPerLine()),
        .dst_x = 0,
        .dst_y = 0,
        .src_box = {},
    };
    if (!wlr_texture_read_pixels(texture.get(), &options))
        qFatal("Could not read capture test output pixels as ARGB8888");
    return image;
}

class CaptureRenderObserver : public QObject
{
public:
    explicit CaptureRenderObserver(WBufferRenderer *renderer) : QObject(renderer)
    {
        connect(renderer, &WBufferRenderer::beforeRendering, this, &CaptureRenderObserver::beforeRender);
        connect(renderer, &WBufferRenderer::afterRendering, this, &CaptureRenderObserver::afterRender);
    }

private:
    void beforeRender()
    {
        m_matrices = g_target
            ? nodeMatrices(QQuickItemPrivate::get(g_target.data())->itemNode())
            : QHash<QSGNode *, QMatrix4x4>();
    }

    void afterRender()
    {
        if (!g_target || m_matrices.isEmpty())
            return;
        ++g_captureFramesChecked;
        const auto current = nodeMatrices(QQuickItemPrivate::get(g_target.data())->itemNode());
        for (auto it = m_matrices.cbegin(); it != m_matrices.cend(); ++it) {
            const auto matrix = current.constFind(it.key());
            if (matrix != current.cend() && *matrix != it.value()) {
                if (g_renderStateUnchanged) {
                    std::fprintf(stderr, "capture fixture: live QSG node %p translation %.1f,%.1f -> %.1f,%.1f\n",
                                 static_cast<void *>(it.key()), it.value()(0, 3), it.value()(1, 3),
                                 (*matrix)(0, 3), (*matrix)(1, 3));
                }
                g_renderStateUnchanged = false;
            }
        }
    }

    QHash<QSGNode *, QMatrix4x4> m_matrices;
};

void observeScreenFrame()
{
    if (!g_tracking || !g_target || g_screen.isNull())
        return;
    const auto current = readScreen();
    if (current == g_screen)
        return;
    if (g_screenUnchanged && current.size() == g_screen.size()) {
        for (int y = 0; y < current.height(); ++y) {
            bool found = false;
            for (int x = 0; x < current.width(); ++x) {
                if (current.pixel(x, y) != g_screen.pixel(x, y)) {
                    std::fprintf(stderr, "capture fixture: output pixel %d,%d %08x -> %08x; window %.1f,%.1f\n",
                                 x, y, g_screen.pixel(x, y), current.pixel(x, y), g_target->x(), g_target->y());
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }
    }
    g_screenUnchanged = false;
}

void observeViewport(WOutputViewport *viewport)
{
    if (viewport->offscreen()) {
        auto *renderer = viewport->findChild<WBufferRenderer *>();
        Q_ASSERT(renderer);
        new CaptureRenderObserver(renderer);
    } else {
        QObject::connect(viewport, &WOutputViewport::frameRendered,
                         WaylandServer::instance(), observeScreenFrame);
    }
}

bool isCaptureViewport(WOutputViewport *viewport)
{
    auto *item = qobject_cast<WSurfaceItem *>(viewport->input());
    return g_target && viewport->offscreen() && !viewport->hideSource() && item
        && item->surface() == g_target->surface();
}

void saveGeometry()
{
    auto *item = g_target->surfaceItem();
    g_position = g_target->position();
    g_size = g_target->size();
    g_parent = g_target->parentItem();
    g_scale = g_target->scale();
    g_visible = g_target->isVisible();
    g_surfacePosition = item->position();
    g_surfaceSize = item->size();
    g_surfaceParent = item->parentItem();
    g_surfaceScale = item->scale();
    g_surfaceVisible = item->isVisible();
    g_scenePosition = item->mapToScene(QPointF());
    g_screenUnchanged = true;
    g_captureFramesChecked = 0;
    g_tracking = true;
}

void readState(void *data)
{
    auto *state = static_cast<toplevel_capture_state *>(data);
    *state = {};
    if (!g_target || !g_target->surfaceItem())
        return;
    auto *item = g_target->surfaceItem();
    state->valid = g_target->surface() && g_target->surface()->mapped();
    state->titlebar_height = g_target->titleBar() ? qRound(g_target->titleBar()->height()) : 0;
    state->width = qRound(item->width());
    state->height = qRound(item->height());
    const QRect bounds = item->boundingRect().toAlignedRect();
    state->bounds_x = bounds.x();
    state->bounds_y = bounds.y();
    state->bounds_width = bounds.width();
    state->bounds_height = bounds.height();
    state->geometry_unchanged = g_target->position() == g_position && g_target->size() == g_size
        && g_target->parentItem() == g_parent && g_target->scale() == g_scale
        && g_target->isVisible() == g_visible && item->position() == g_surfacePosition
        && item->size() == g_surfaceSize && item->parentItem() == g_surfaceParent
        && item->scale() == g_surfaceScale && item->isVisible() == g_surfaceVisible
        && item->mapToScene(QPointF()) == g_scenePosition;
    const QImage current = readScreen();
    state->screen_unchanged = g_screenUnchanged && !g_screen.isNull() && current == g_screen;
    state->render_state_unchanged = g_renderStateUnchanged;
    state->capture_frames_checked = g_captureFramesChecked;
    state->maximized = g_target->isMaximized();
    state->dragging = WaylandServer::instance()->rootSurfaceContainer()->moveResizeSurface() == g_target;
    state->border_visible = !g_target->noDecoration() && g_target->visibleDecoration()
        && g_target->borderVisible();
    state->position_x = qRound(g_target->x());
    state->position_y = qRound(g_target->y());
    if (g_tracking && !state->geometry_unchanged) {
        std::fprintf(stderr, "capture fixture: position %.1f,%.1f -> %.1f,%.1f; scene %.1f,%.1f -> %.1f,%.1f\n",
                     g_position.x(), g_position.y(), g_target->x(), g_target->y(),
                     g_scenePosition.x(), g_scenePosition.y(),
                     item->mapToScene(QPointF()).x(), item->mapToScene(QPointF()).y());
    }
    if (g_tracking && !state->screen_unchanged && !qEnvironmentVariableIsEmpty("TREELAND_CAPTURE_TEST_IMAGE")) {
        g_screen.save(QString::fromLocal8Bit(qgetenv("TREELAND_CAPTURE_TEST_IMAGE")) + ".before.png");
        current.save(QString::fromLocal8Bit(qgetenv("TREELAND_CAPTURE_TEST_IMAGE")) + ".after.png");
    }
    const auto viewports = WaylandServer::instance()->window()->contentItem()->findChildren<WOutputViewport *>();
    for (auto *viewport : viewports) {
        if (isCaptureViewport(viewport)) {
            ++state->offscreen_viewports;
            auto *input = qobject_cast<WSurfaceItem *>(viewport->input());
            if (input != item && !input->eventItem())
                ++state->isolated_inputs;
        }
    }
    // The occluder covers this target-content point in the actual output.
    // Its magenta pixel must never appear in the isolated window capture.
    const QPoint covered = item->mapToScene(QPointF(100, state->titlebar_height + 90)).toPoint();
    state->occluder_visible = current.rect().contains(covered)
        && current.pixelColor(covered) == QColor(255, 0, 255);
}

void pointerMotion(int x, int y)
{
    wlr_pointer_motion_absolute_event event {
        .pointer = g_pointer,
        .time_msec = ++g_inputTime,
        .x = x / 800.0,
        .y = y / 600.0,
    };
    wl_signal_emit_mutable(&g_pointer->events.motion_absolute, &event);
    wl_signal_emit_mutable(&g_pointer->events.frame, nullptr);
}

void pointerButton(wl_pointer_button_state state)
{
    wlr_pointer_button_event event {
        .pointer = g_pointer,
        .time_msec = ++g_inputTime,
        .button = 0x110,
        .state = state,
    };
    wl_signal_emit_mutable(&g_pointer->events.button, &event);
    wl_signal_emit_mutable(&g_pointer->events.frame, nullptr);
}
}

void protocol_test_setup(WaylandServer *server)
{
#ifdef TREELAND_CAPTURE_TEST_GPU
    auto *renderer = server->window()->renderer();
    if (!wlr_renderer_is_gles2(renderer) || wlr_renderer_get_drm_fd(renderer) < 0)
        qFatal("GPU capture test must use the GLES2 renderer, not a software fallback");
#endif
    // The framework disables the wallpaper process. Supply an opaque scene
    // background so the software renderer's preserved buffers can repaint
    // exposed pixels, rather than accumulate shadows over stale window pixels.
    QQmlComponent background(server->qmlEngine());
    background.setData(
        "import QtQuick; Rectangle { width: 800; height: 600; color: '#102030'; z: -1 }",
        QUrl());
    auto *item = qobject_cast<QQuickItem *>(background.create());
    if (!item)
        qFatal("Could not create capture test background");
    item->setParent(server->window()->contentItem());
    item->setParentItem(server->window()->contentItem());

    wlr_backend *headless = nullptr;
    wlr_multi_for_each_backend(
        server->backend()->handle(),
        [](wlr_backend *backend, void *data) {
            if (wlr_backend_is_headless(backend))
                *static_cast<wlr_backend **>(data) = backend;
        },
        &headless);
    // Let OutputManager register and enable the output after its configuration
    // is ready. Enabling it here makes the framework add it to the layout too
    // early, then production removes/re-adds it while cursor layers are live.
    if (!headless || !wlr_headless_add_output(headless, 800, 600))
        qFatal("Could not add capture test output");
    server->window()->setDisableLayers(true);
    server->setAnimationSpeed(0.1f);
    attach_test_pointer_device(server);
    for (auto *device : server->seat()->deviceList()) {
        if (device->type() == WInputDevice::Type::Pointer)
            g_pointer = wlr_pointer_from_input_device(device->handle());
    }
    Q_ASSERT(g_pointer);
    server->seat()->cursor()->setVisible(false);
    QObject::connect(server->window(), &WOutputRenderWindow::outputViewportInitialized,
                     server, observeViewport);
    QObject::connect(server->shellHandler(), &ShellHandler::surfaceWrapperAdded, server,
                     [](SurfaceWrapper *wrapper) { wrapper->disableWindowAnimation(); });
}

#ifdef TREELAND_CAPTURE_TEST_GPU
extern "C" bool protocol_test_preflight()
{
    const QDir dri(QStringLiteral("/dev/dri"));
    const auto nodes = dri.entryList({QStringLiteral("renderD*")}, QDir::System);
    for (const auto &node : nodes) {
        const auto path = dri.filePath(node).toLocal8Bit();
        const int fd = open(path.constData(), O_RDWR | O_CLOEXEC);
        if (fd >= 0) {
            close(fd);
            std::fprintf(stderr, "GPU capture test: accessible DRM render node %s\n", path.constData());
            return true;
        }
    }
    std::fprintf(stderr, "GPU capture test skipped: no accessible DRM render node\n");
    return false;
}
#endif

extern "C" void toplevel_capture_prepare(void *data)
{
    for (auto *wrapper : WaylandServer::instance()->rootSurfaceContainer()->surfaces()) {
        if (wrapper->appId() == QLatin1String("capture-target"))
            g_target = wrapper;
        else if (wrapper->appId() == QLatin1String("capture-occluder"))
            g_occluder = wrapper;
    }
    if (!g_target || !g_occluder) {
        *static_cast<toplevel_capture_state *>(data) = {};
        return;
    }
    g_tracking = false;
    g_target->setPositionAutomatic(false);
    g_target->setPosition(QPointF(100, 90));
    g_occluder->setPositionAutomatic(false);
    g_occluder->setPosition(QPointF(170, 170));
    g_occluder->stackToLast();
    g_occluder->setZ(10);
    WaylandServer::instance()->activateSurface(g_target);
    screenViewport()->render(true);
    saveGeometry();
    g_screen = readScreen();
    readState(data);
}

extern "C" void toplevel_capture_check_screen(void *data)
{
    readState(data);
}

extern "C" void toplevel_capture_repaint_screen(void *data)
{
    screenViewport()->render(true);
    readState(data);
}

extern "C" void toplevel_capture_save_screen(void *data)
{
    g_tracking = false;
    screenViewport()->render(true);
    saveGeometry();
    g_screen = readScreen();
    readState(data);
}

extern "C" void toplevel_capture_render_offscreen(void *data)
{
    auto *output = screenViewport()->output()->handle();
    const uint32_t commits = output->commit_seq;
    const auto viewports = WaylandServer::instance()->window()->contentItem()->findChildren<WOutputViewport *>();
    for (auto *viewport : viewports) {
        if (isCaptureViewport(viewport) && viewport->isVisible())
            viewport->render(false);
    }
    readState(data);
    static_cast<toplevel_capture_state *>(data)->capture_only_did_not_commit = output->commit_seq == commits;
}

extern "C" void toplevel_capture_move_partly_offscreen(void *data)
{
    // No standard xdg request positions a toplevel; this is a directed fixture
    // move, not a fabricated capture result. The capture must remain complete.
    g_tracking = false;
    g_target->setPosition(QPointF(-150, 210));
    toplevel_capture_save_screen(data);
}

extern "C" void toplevel_capture_pause_checks(void *)
{
    // Expected scene changes must not be mistaken for capture side effects.
    // The per-render QSG observer stays active, including during animations.
    g_tracking = false;
}

extern "C" void toplevel_capture_move_window(void *data)
{
    auto *move = static_cast<toplevel_capture_move *>(data);
    g_tracking = false;
    g_target->setPosition(QPointF(move->x, move->y));
    toplevel_capture_save_screen(&move->state);
}

extern "C" void toplevel_capture_wait_transition(void *data)
{
    g_tracking = false;
    auto *window = WaylandServer::instance()->window();
    window->render();
    if (g_target->isAnimationRunning()) {
        QQuickItem *animation = nullptr;
        for (auto *child : g_target->container()->childItems()) {
            if (child->property("surface").value<SurfaceWrapper *>() == g_target) {
                animation = child;
                break;
            }
        }
        if (!animation)
            qFatal("Could not observe capture target geometry animation");
        QEventLoop loop;
        QObject::connect(animation, SIGNAL(finished()), &loop, SLOT(quit()));
        loop.exec();
    }
    readState(data);
}

extern "C" void toplevel_capture_drag_begin(void *data)
{
    g_tracking = false;
    // Use the real SSD DragHandler -> moveRequested -> per-seat restore path,
    // not an unmaximize request or a fixture write to the window state.
    pointerMotion(300, 15);
    pointerButton(WL_POINTER_BUTTON_STATE_PRESSED);
    pointerMotion(335, 55);
    readState(data);
}

extern "C" void toplevel_capture_drag_move(void *data)
{
    auto *move = static_cast<toplevel_capture_move *>(data);
    g_tracking = false;
    pointerMotion(move->x, move->y);
    toplevel_capture_save_screen(&move->state);
}

extern "C" void toplevel_capture_drag_end(void *data)
{
    g_tracking = false;
    pointerButton(WL_POINTER_BUTTON_STATE_RELEASED);
    toplevel_capture_save_screen(data);
}
