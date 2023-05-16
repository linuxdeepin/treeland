/*
 * Copyright (C) 2021 ~ 2022 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "woutput.h"
#include "wbackend.h"
#include "woutputlayout.h"
#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
#include "woutputdamage.h"
#endif
#include "wsignalconnector.h"
#include "wcursor.h"
#include "wseat.h"
#include "wquickrendercontrol.h"
#include "wthreadutils.h"
#include "utils/wtools.h"
#include "platformplugin/qwlrootscreen.h"

#include <qwoutput.h>
#include <qwoutputlayout.h>
#include <qwrenderer.h>

extern "C" {
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
#include <wlr/types/wlr_output_damage.h>
#endif
#include <wlr/backend.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/egl.h>
#define static
#include <wlr/render/gles2.h>
#undef static
#include <wlr/render/pixman.h>
#ifdef ENABLE_VULKAN_RENDER
#include <wlr/render/vulkan.h>
#endif
#include <wlr/util/region.h>
}

#include <QSurface>
#define private public
#include <QScreen>
#include <QQuickRenderControl>
#include <QOpenGLContext>
#include <QOpenGLContextGroup>
#include <qpa/qplatformsurface.h>
#include <qpa/qplatformopenglcontext.h>
#include <private/qopenglcontext_p.h>
#include <private/qquickrendercontrol_p.h>
#include <private/qquickwindow_p.h>
#undef private
#include <QOpenGLFunctions>
#include <QOffscreenSurface>
#include <QCoreApplication>
#include <QThread>
#include <QQuickWindow>
#include <QPixelFormat>
#include <QDebug>
#include <QCursor>

#include <qpa/qplatformscreen.h>
#include <qpa/qplatformcursor.h>
#include <qpa/qwindowsysteminterface_p.h>
#include <private/qsgrenderer_p.h>
#include <private/qsgrhisupport_p.h>
#include <private/qquickwindow_p.h>
#include <private/qrhi_p.h>
#include <private/qeglconvenience_p.h>
#include <private/qsgsoftwarerenderer_p.h>
#include <private/qguiapplication_p.h>
#ifdef ENABLE_VULKAN_RENDER
#include <private/qvulkaninstance_p.h>
#include <private/qbasicvulkanplatforminstance_p.h>
#endif

#include <EGL/egl.h>
#include <drm_fourcc.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

struct QScopedPointerPixmanRegion32Deleter {
    static inline void cleanup(pixman_region32_t *pointer) {
        if (pointer)
            pixman_region32_fini(pointer);
        delete pointer;
    }
};

typedef QScopedPointer<pixman_region32_t, QScopedPointerPixmanRegion32Deleter> pixman_region32_scoped_pointer;

class Q_DECL_HIDDEN WOutputPrivate : public WObjectPrivate
{
public:
    WOutputPrivate(WOutput *qq, void *handle)
        : WObjectPrivate(qq)
        , handle(reinterpret_cast<QWOutput*>(handle))
    {
        this->handle->handle()->data = qq;
    }

    ~WOutputPrivate() {
        handle->handle()->data = nullptr;
        if (layout)
            layout->remove(q_func());
    }

    // begin slot function
#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    void on_damage_frame(void *);
#else
    void on_frame();
    void on_damage();
#endif
    void on_mode(void*);
    // end slot function

    void init();
    bool initRCWithRhi();
    void connect();
    void updateProjection();
    void maybeUpdateRenderTarget();
    void doSetCursor(const QCursor &cur);
    void setCursor(const QCursor &cur);

    inline QCursor &ensureCursor();

    inline void processInputEvent(WInputEvent *event) {
        currentInputEvent = event;
        auto system_event = static_cast<QEvent*>(event->data->nativeEvent);
        QCoreApplication::sendEvent(q_func()->attachedWindow(), system_event);
        delete system_event;
        currentInputEvent = nullptr;
    }

    inline void emitSizeChanged() {
        W_Q(WOutput);
        auto screen = rc->window()->screen();
        const QRect geo(q->position(), q->size());
        QWindowSystemInterface::handleScreenGeometryChange(screen, geo, geo);
        Q_EMIT q->sizeChanged();
    }

    inline QSize size() const {
        Q_ASSERT(handle);
        return QSize(handle->handle()->width, handle->handle()->height);
    }

    inline QSize transformedSize() const {
        Q_ASSERT(handle);
        return handle->transformedResolution();
    }

    inline QSize effectiveSize() const {
        Q_ASSERT(handle);
        return handle->effectiveResolution();
    }

    inline WOutput::Transform orientation() const {
        return static_cast<WOutput::Transform>(handle->handle()->transform);
    }

    inline QWRenderer *renderer() const {
        return backend->rendererNativeInterface<QWRenderer>();
    }

    inline QWAllocator *allocator() const {
        return reinterpret_cast<QWAllocator*>(backend->allocator());
    }

    inline bool bufferIsAttached() const {
        return attachedBuffer;
    }

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    inline bool attachRender(pixman_region32_t *damage, bool *needsFrame = nullptr) {
        if (bufferIsAttached()) {
            // Don't reuse the state, needs ensuse the damage data is valid
            handle->rollback();
        }

        bool needs_frame;
        if (!wlr_output_damage_attach_render(this->damage->nativeInterface<wlr_output_damage>(),
                &needs_frame, damage)) {
            return false;
        }

        if (needsFrame)
            *needsFrame = needs_frame;

        attachedBuffer = true;
        return true;
    }
#endif

    inline bool attachRender() {
        if (bufferIsAttached())
            return true;

        if (!handle->attachRender(nullptr))
            return false;

        attachedBuffer = true;
        return true;
    }

    inline void detachRender() {
        attachedBuffer = false;
        handle->rollback();
    }

    inline bool makeCurrent() {
        if (context) {
            if (QOpenGLContext::currentContext() == context)
                return true;
            return context->makeCurrent(rc->window());
        } else {
            return attachRender();
        }
    }

    inline void doneCurrent() {
        if (context) {
            context->doneCurrent();
        } else {
            detachRender();
        }
    }

    bool doRender();

    inline void render() {
#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
        damage->addWhole();
#else
        contentIsDirty = true;
#endif
        doRender();
    }

    inline void rotate(WOutput::Transform t) {
        handle->setTransform(t);
        updateProjection();
    }
    inline void setScale(float scale) {
        handle->setScale(scale);
        updateProjection();
    }

    W_DECLARE_PUBLIC(WOutput)

    QPointer<QWOutput> handle;
    QWlrootsScreen *screen = nullptr;

    WBackend *backend = nullptr;
    QQuickRenderControl *rc = nullptr;
    QOpenGLContext *context = nullptr;
    WOutputLayout *layout = nullptr;
#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    WOutputDamage *damage = nullptr;
#endif
    QVector<WCursor*> cursorList;
    QScopedPointer<QCursor> cursor;

    // for software renderer
    QImage paintDevice;
    pixman_image_t *currentBuffer = nullptr;

#ifdef ENABLE_VULKAN_RENDER
    QScopedPointer<QVulkanInstance> vkInstance;
#endif

    bool renderable = false;
#ifdef WAYLIB_DISABLE_OUTPUT_DAMAGE
    bool contentIsDirty = false;
#endif
    bool attachedBuffer = false;

    // in attached window thread
    WInputEvent *currentInputEvent = nullptr;

    QMatrix4x4 projection;

    WSignalConnector sc;
};

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
void WOutputPrivate::on_damage_frame(void *)
{
    renderable = true;
    doRender();
}
#else
void WOutputPrivate::on_frame()
{
    renderable = true;
    doRender();
}

void WOutputPrivate::on_damage()
{
    contentIsDirty = true;
    doRender();
}
#endif

void WOutputPrivate::on_mode(void *)
{
    W_Q(WOutput);
    emitSizeChanged();
    Q_EMIT q->transformedSizeChanged(transformedSize());
    Q_EMIT q->effectiveSizeChanged(effectiveSize());
    updateProjection();
}

void WOutputPrivate::init()
{
    W_Q(WOutput);

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    damage = new WOutputDamage(q);
#endif

    connect();

    updateProjection();

#ifdef ENABLE_VULKAN_RENDER
    if (wlr_renderer_is_vk(renderer()->handle())) {
        bool initOK = initRCWithRhi();
        Q_ASSERT(initOK);
    } else
#endif
    if (wlr_renderer_is_gles2(renderer()->handle())) {
        bool initOK = initRCWithRhi();
        Q_ASSERT(initOK);
    } else {
        qFatal("Not supported renderer");
    }

    /* Ensure call the "requestRender" on later via Qt::QueuedConnection, if not will crash
    at "Q_ASSERT(prevDirtyItem)" in the QQuickItem, because the QQuickRenderControl::render
    will trigger the QQuickWindow::sceneChanged signal that trigger the
    QQuickRenderControl::sceneChanged, this is a endless loop calls.

    Functions call list:
    0. WOutput::requestRender
    1. QQuickRenderControl::render
    2. QQuickWindowPrivate::renderSceneGraph
    3. QQuickItemPrivate::addToDirtyList
    4. QQuickWindowPrivate::dirtyItem
    5. QQuickWindow::maybeUpdate
    6. QQuickRenderControlPrivate::maybeUpdate
    7. QQuickRenderControl::sceneChanged
    */
    // TODO: Get damage regions from the Qt, and use WOutputDamage::add instead of WOutput::requestRender.
    QObject::connect(rc, &QQuickRenderControl::renderRequested,
                     q, &WOutput::requestRender, Qt::QueuedConnection);
    QObject::connect(rc, &QQuickRenderControl::sceneChanged,
                     q, &WOutput::requestRender, Qt::QueuedConnection);

    // init cursor
    doSetCursor(ensureCursor());
    render();
}

inline static QByteArrayList fromCStyleList(size_t count, const char **list) {
    QByteArrayList al;
    al.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        al.append(list[i]);
    }
    return al;
}

bool WOutputPrivate::initRCWithRhi()
{
    QQuickRenderControlPrivate *rcd = QQuickRenderControlPrivate::get(rc);
    QSGRhiSupport *rhiSupport = QSGRhiSupport::instance();

    // sanity check for Vulkan
#ifdef ENABLE_VULKAN_RENDER
    if (rhiSupport->rhiBackend() == QRhi::Vulkan) {
        vkInstance.reset(new QVulkanInstance());

        auto phdev = wlr_vk_renderer_get_physical_device(renderer()->handle());
        auto dev = wlr_vk_renderer_get_device(renderer()->handle());
        auto queue_family = wlr_vk_renderer_get_queue_family(renderer()->handle());

#if QT_VERSION > QT_VERSION_CHECK(6, 6, 0)
        auto instance = wlr_vk_renderer_get_instance(renderer()->handle());
        vkInstance->setVkInstance(instance);
#endif
//        vkInstance->setExtensions(fromCStyleList(vkRendererAttribs.extension_count, vkRendererAttribs.extensions));
//        vkInstance->setLayers(fromCStyleList(vkRendererAttribs.layer_count, vkRendererAttribs.layers));
        vkInstance->setApiVersion({1, 1, 0});
        vkInstance->create();
        rcd->window->setVulkanInstance(vkInstance.data());

        auto gd = QQuickGraphicsDevice::fromDeviceObjects(phdev, dev, queue_family);
        rcd->window->setGraphicsDevice(gd);
    } else
#endif
    if (rhiSupport->rhiBackend() == QRhi::OpenGLES2) {
        context = new QOpenGLContext(rc);
        context->setObjectName(QT_STRINGIFY(WAYLIB_SERVER_NAMESPACE));
        context->setScreen(rc->window()->screen());
        bool ok = context->create();
        if (!ok)
            return false;

        rcd->window->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(context));
    } else {
        qFatal("Not supported RHI backend: %s", qPrintable(rhiSupport->rhiBackendName()));
    }

    QSGRhiSupport::RhiCreateResult result = rhiSupport->createRhi(rcd->window, rcd->window);
    if (!result.rhi) {
        qWarning("WOutput::initRhi: Failed to initialize QRhi");
        return false;
    }

    rcd->rhi = result.rhi;
    // Ensure the QQuickRenderControl don't reinit the RHI
    rcd->ownRhi = true;
    if (!rc->initialize())
        return false;
    rcd->ownRhi = result.own;
    Q_ASSERT(rcd->rhi == result.rhi);

    return true;
}

void WOutputPrivate::connect()
{
#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    sc.connect(&damage->nativeInterface<wlr_output_damage>()->events.frame,
               this, &WOutputPrivate::on_damage_frame);
    sc.connect(&damage->nativeInterface<wlr_output_damage>()->events.destroy,
               &sc, &WSignalConnector::invalidate);
#else
    QObject::connect(handle, &QWOutput::frame, q_func(), [this] {
        on_frame();
    });
    QObject::connect(handle, &QWOutput::damage, q_func(), [this] {
        on_damage();
    });
#endif
    // In call the connect for 'frame' signal before, maybe the wlr_output object is already
    // emit the signal, so we should suppose the renderable is true in order that ensure can
    // render on the next time
    renderable = true;

    // On the X11/Wayalnd mode，when the window size of 'output' is changed then will trigger
    // the 'mode' signal
//    sc.connect(&handle->events.mode,
//               this, &WOutputPrivate::on_mode);
}

void WOutputPrivate::updateProjection()
{
    projection = QMatrix4x4();
    projection.rotate(-90 * handle->handle()->pending.transform, 0, 0, 1);

    QSizeF size = handle->handle()->pending.transform % 2 ? QSize(handle->handle()->height, handle->handle()->width)
                                                       : this->size();

    if (handle->handle()->pending.scale > 0) {
        size /= handle->handle()->pending.scale;

        if (!paintDevice.isNull())
            paintDevice.setDevicePixelRatio(handle->handle()->pending.scale);
    }

    QRectF rect(QPointF(0, 0), size);

    if (Q_LIKELY(true)) {
        projection.ortho(rect.x(),
                         rect.x() + rect.width(),
                         rect.y(),
                         rect.y() + rect.height(),
                         1,
                         -1);
    } else {
        projection.ortho(rect.x(),
                         rect.x() + rect.width(),
                         rect.y() + rect.height(),
                         rect.y(),
                         1,
                         -1);
    }
}

void WOutputPrivate::maybeUpdateRenderTarget()
{
    if (wlr_renderer_is_pixman(renderer()->handle())) {
        pixman_image_t *image = wlr_pixman_renderer_get_current_image(renderer()->handle());
        void *data = pixman_image_get_data(image);

        if (Q_LIKELY(paintDevice.constBits() == data))
            return;

        paintDevice = WTools::fromPixmanImage(image, data);
        Q_ASSERT(!paintDevice.isNull());
        paintDevice.setDevicePixelRatio(handle->handle()->pending.scale);

        auto rt = QQuickRenderTarget::fromPaintDevice(&paintDevice);
        rc->QQuickRenderControl::window()->setRenderTarget(rt);
    }
#ifdef ENABLE_VULKAN_RENDER
    else if (vkInstance) {
        wlr_vk_image_attribs attribs;
        wlr_vk_renderer_get_current_image_attribs(renderer()->handle(), &attribs);
        auto rt = QQuickRenderTarget::fromVulkanImage(attribs.image, attribs.layout, attribs.format, size());
        rc->QQuickRenderControl::window()->setRenderTarget(rt);
    }
#endif
    else if (context) {
        GLint rbo = 0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &rbo);

        if (rbo) {
            auto rt = QQuickRenderTarget::fromOpenGLRenderBuffer(rbo, size());
            rt.setMirrorVertically(true);
            rc->QQuickRenderControl::window()->setRenderTarget(rt);
        }
    }
}

void WOutputPrivate::doSetCursor(const QCursor &cur)
{
    ensureCursor() = cur;

    // TODO: should to find which related QQuickItem for the cursor
    Q_FOREACH(auto wlr_cursor, cursorList) {
        wlr_cursor->setCursor(ensureCursor());
    }
}

void WOutputPrivate::setCursor(const QCursor &cur)
{
    backend->server()->threadUtil()->run(this, &WOutputPrivate::doSetCursor, cur);
}

QCursor &WOutputPrivate::ensureCursor()
{
    if (Q_UNLIKELY(!cursor))
        cursor.reset(new QCursor(WCursor::defaultCursor()));

    return *cursor.get();
}

bool WOutputPrivate::doRender()
{
    if (!renderable) {
        return false;
    }

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    pixman_region32_scoped_pointer damage(new pixman_region32_t);
    pixman_region32_init(damage.data());
    if (!attachRender(damage.data()))
        return false;
#else
    if (!contentIsDirty) {
        return false;
    }
#endif

    if (Q_UNLIKELY(!makeCurrent()))
        return false;

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    if (pixman_region32_not_empty(damage.data()))
#else
    contentIsDirty = false;
#endif
    {
        // TODO: In Qt GUI thread
        rc->polishItems();

        rc->beginFrame();
        rc->sync();

        maybeUpdateRenderTarget();
        // TODO: use scissor with the damage regions for render.
        rc->render();
        rc->endFrame();
    }

    if (!handle->handle()->hardware_cursor) {
        renderer()->begin(handle->handle()->width, handle->handle()->height);
        /* Hardware cursors are rendered by the GPU on a separate plane, and can be
         * moved around without re-rendering what's beneath them - which is more
         * efficient. However, not all hardware supports hardware cursors. For this
         * reason, wlroots provides a software fallback, which we ask it to render
         * here. wlr_cursor handles configuring hardware vs software cursors for you,
         * and this function is a no-op when hardware cursors are in use. */
        handle->renderSoftwareCursors(nullptr);
        renderer()->end();
    }

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    const QSize size = transformedSize();
    enum wl_output_transform transform =
        wlr_output_transform_invert(handle->handle()->transform);

    pixman_region32_scoped_pointer frame_damage(new pixman_region32_t);
    pixman_region32_init(frame_damage.data());
    wlr_region_transform(frame_damage.data(), &this->damage->nativeInterface<wlr_output_damage>()->current,
                         transform, size.width(), size.height());
    handle->setDamage(frame_damage.data());
    pixman_region32_fini(frame_damage.data());
#endif

    auto dirtyFlags = handle->handle()->pending.committed;
    bool ret = handle->commit();
    doneCurrent();

    renderable = false;

    if (Q_LIKELY(ret)) {
        W_Q(WOutput);

        if (Q_UNLIKELY(dirtyFlags & WLR_OUTPUT_STATE_MODE)) {
            emitSizeChanged();
        }

        if (Q_UNLIKELY(dirtyFlags & WLR_OUTPUT_STATE_TRANSFORM)) {
            auto screen = rc->window()->screen();
            QWindowSystemInterface::handleScreenOrientationChange(screen,
                                                                  screen->handle()->orientation());
            Q_EMIT q->orientationChanged();
        }

        if (Q_UNLIKELY(dirtyFlags & (WLR_OUTPUT_STATE_MODE
                                     | WLR_OUTPUT_STATE_TRANSFORM))) {
            Q_EMIT q->transformedSizeChanged(transformedSize());
        }

        if (Q_UNLIKELY(dirtyFlags & WLR_OUTPUT_STATE_SCALE)) {
            Q_EMIT q->scaleChanged();
            auto screen = rc->window()->screen();
            // Notify for the window device pixel ratio to changed
            QWindowSystemInterface::handleScreenLogicalDotsPerInchChange(screen,
                                                                         screen->logicalDotsPerInchX(),
                                                                         screen->logicalDotsPerInchY());
        }

        if (Q_UNLIKELY(dirtyFlags & (WLR_OUTPUT_STATE_MODE
                                     | WLR_OUTPUT_STATE_TRANSFORM
                                     | WLR_OUTPUT_STATE_SCALE))) {
            Q_EMIT q->effectiveSizeChanged(effectiveSize());
            // need reset cursor
            doSetCursor(ensureCursor());
        }
    }

    return ret;
}

WOutput::WOutput(WOutputHandle *handle, WBackend *backend)
    : QObject()
    , WObject(*new WOutputPrivate(this, handle))
{
    d_func()->backend = backend;
}

WOutput::~WOutput()
{
    detach();
}

WBackend *WOutput::backend() const
{
    W_DC(WOutput);
    return d->backend;
}

bool WOutput::isValid() const
{
    W_DC(WOutput);
    return d->rc;
}

void WOutput::attach(QQuickRenderControl *control)
{
    W_D(WOutput);

    d->rc = control;
    d->rc->prepareThread(backend()->server()->thread());
    d->init();
}

void WOutput::detach()
{
    W_D(WOutput);

    if (d->rc) {
        // Call the "rc->invalidate()" will trigger the "sceneChanged" signal
        d->rc->disconnect(this);

        auto w = attachedWindow();
        w->destroy();
        w->setScreen(nullptr);

        d->rc->invalidate();
        delete d->rc;
        d->rc = nullptr;
    }

    d->currentBuffer = nullptr;
}

WOutputHandle *WOutput::handle() const
{
    W_DC(WOutput);
    return reinterpret_cast<WOutputHandle*>(d->handle.data());
}

WOutput *WOutput::fromHandle(const WOutputHandle *handle)
{
    auto wlr_handle = reinterpret_cast<const QWOutput*>(handle)->handle();
    return reinterpret_cast<WOutput*>(wlr_handle->data);
}

WOutput *WOutput::fromScreen(const QScreen *screen)
{
    return fromHandle(static_cast<QWlrootsScreen*>(screen->handle())->output());
}

WOutput *WOutput::fromWindow(const QWindow *window)
{
    return fromScreen(window->screen());
}

void WOutput::requestRender()
{
    W_D(WOutput);
    if (!d->handle)
        return;
    d->render();
}

void WOutput::setScreen(QWlrootsScreen *screen)
{
    W_D(WOutput);
    d->screen = screen;
}

QWlrootsScreen *WOutput::screen() const
{
    W_DC(WOutput);
    return d->screen;
}

void WOutput::rotate(Transform t)
{
    W_D(WOutput);

    d->rotate(t);
}

void WOutput::setScale(float scale)
{
    W_D(WOutput);

    d->setScale(scale);
}

QPoint WOutput::position() const
{
    W_DC(WOutput);

    QPoint p;

    if (Q_UNLIKELY(!d->layout))
        return p;

    auto l_output = d->layout->nativeInterface<QWOutputLayout>()->get(d->handle->handle());

    if (Q_UNLIKELY(!l_output))
        return p;

    return QPoint(l_output->x, l_output->y);
}

QSize WOutput::size() const
{
    W_DC(WOutput);

    return d->size();
}

QSize WOutput::transformedSize() const
{
    W_DC(WOutput);

    return d->transformedSize();
}

QSize WOutput::effectiveSize() const
{
    W_DC(WOutput);

    return d->effectiveSize();
}

WOutput::Transform WOutput::orientation() const
{
    W_DC(WOutput);

    return d->orientation();
}

float WOutput::scale() const
{
    W_DC(WOutput);

    return d->handle->handle()->scale;
}

QQuickWindow *WOutput::attachedWindow() const
{
    W_DC(WOutput);

    if (Q_UNLIKELY(!d->rc))
        return nullptr;
    return d->rc->window();
}

void WOutput::setLayout(WOutputLayout *layout)
{
    W_D(WOutput);

    if (d->layout == layout)
        return;

    d->layout = layout;
}

WOutputLayout *WOutput::layout() const
{
    W_DC(WOutput);

    return d->layout;
}

void WOutput::cursorEnter(WCursor *cursor)
{
    W_D(WOutput);
    Q_ASSERT(!d->cursorList.contains(cursor));
    d->cursorList << cursor;

    if (isValid())
        cursor->setCursor(d->ensureCursor());
}

void WOutput::cursorLeave(WCursor *cursor)
{
    W_D(WOutput);
    Q_ASSERT(d->cursorList.contains(cursor));
    d->cursorList.removeOne(cursor);
}

QVector<WCursor *> WOutput::cursorList() const
{
    W_DC(WOutput);
    return d->cursorList;
}

void WOutput::postInputEvent(WInputEvent *event)
{
    W_D(WOutput);

    d->processInputEvent(event);
    delete event;
}

WInputEvent *WOutput::currentInputEvent() const
{
    W_DC(WOutput);
    return d->currentInputEvent;
}

QCursor WOutput::cursor() const
{
    W_DC(WOutput);
    return d->cursor.isNull() ? WCursor::defaultCursor() : *d->cursor.get();
}

WAYLIB_SERVER_END_NAMESPACE
