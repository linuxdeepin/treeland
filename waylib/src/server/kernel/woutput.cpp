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

extern "C" {
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/backend.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/egl.h>
#define static
#include <wlr/render/gles2.h>
#undef static
#include <wlr/render/pixman.h>
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
#include <private/qquickwindow_p.h>
#include <private/qrhi_p.h>
#include <private/qeglconvenience_p.h>
#include <private/qsgsoftwarerenderer_p.h>
#include <private/qguiapplication_p.h>

#include <EGL/egl.h>
#include <drm_fourcc.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN OutputGLContext : public QOpenGLContext {
public:
    OutputGLContext(WOutputPrivate *output)
        : output(output) {}
    bool create();

private:
    WOutputPrivate *output;
    friend class OutputGLPlatform;
};

class Q_DECL_HIDDEN OutputSurface : public QSurface
{
public:
    class Platform : public QPlatformSurface
    {
    public:
        Platform(OutputSurface *surface)
            : QPlatformSurface(surface) {

        }

        QSurfaceFormat format() const override;
        QPlatformScreen *screen() const override {
            return nullptr;
        }
    };

    OutputSurface(WOutputPrivate *output, SurfaceType type)
        : QSurface(Offscreen)
        , output(output)
        , type(type)
    {

    }

    static inline WOutputPrivate *getOutput(const QPlatformSurface *surface) {
        return static_cast<const OutputSurface*>(surface->surface())->output;
    }

    void create() {
        handle.reset(new Platform(this));
    }

    QSurfaceFormat format() const override {
        return surfaceHandle()->format();
    }
    QPlatformSurface *surfaceHandle() const override {
        return handle.get();
    }

    SurfaceType surfaceType() const override {
        return type;
    }

    QSize size() const override;

private:
    friend class OutputGLPlatform;

    QScopedPointer<Platform> handle;
    WOutputPrivate *output;
    SurfaceType type;
};

class Q_DECL_HIDDEN OutputRenderStage : public QQuickCustomRenderStage
{
public:
    OutputRenderStage(QQuickWindowPrivate *w, const WOutputPrivate *output)
        : window(w), output(output) {}

    bool render() override;
    bool swap() override {
        return false;
    }

private:
    QQuickWindowPrivate *window;
    const WOutputPrivate *output;
};

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
        , handle(reinterpret_cast<wlr_output*>(handle))
    {
        this->handle->data = qq;
    }

    ~WOutputPrivate() {
        handle->data = nullptr;
        if (layout)
            layout->remove(q_func());
    }

    // begin slot function
#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    void on_damage_frame(void *);
#else
    void on_frame(void *);
    void on_damage(void *);
#endif
    void on_mode(void*);
    // end slot function

    void init();
    void connect();
    void updateProjection();
    void maybeUpdatePaintDevice();
    void doSetCursor(const QCursor &cur);
    void setCursor(const QCursor &cur);

    inline QScreen *ensureScreen();
    inline QWindow *ensureRenderWindow();
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
        auto screen = ensureScreen();
        const QRect geo(q->position(), q->size());
        QWindowSystemInterface::handleScreenGeometryChange(screen, geo, geo);
        Q_EMIT q->sizeChanged();
    }

    inline QSize size() const {
        Q_ASSERT(handle);
        return QSize(handle->width, handle->height);
    }

    inline QSize transformedSize() const {
        Q_ASSERT(handle);
        QSize size;
        wlr_output_transformed_resolution(handle, &size.rwidth(), &size.rheight());
        return size;
    }

    inline QSize effectiveSize() const {
        Q_ASSERT(handle);
        QSize size;
        wlr_output_effective_resolution(handle, &size.rwidth(), &size.rheight());
        return size;
    }

    inline WOutput::Transform orientation() const {
        return static_cast<WOutput::Transform>(handle->transform);
    }

    inline wlr_renderer *renderer() const {
        return reinterpret_cast<wlr_renderer*>(backend->renderer());
    }

    inline wlr_allocator *allocator() const {
        return reinterpret_cast<wlr_allocator*>(backend->allocator());
    }

    inline bool needsAttachBuffer() const {
        return !attachedBuffer;
    }

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    inline bool attachRender(pixman_region32_t *damage) {
        if (!needsAttachBuffer())
            return true;

        bool needs_frame;
        if (!wlr_output_damage_attach_render(this->damage->nativeInterface<wlr_output_damage>(),
                &needs_frame, damage)) {
            return false;
        }

        if (!needs_frame) {
            wlr_output_rollback(handle);
            return false;
        }

        attachedBuffer = true;
        return true;
    }

    inline bool attachRender() {
        pixman_region32_scoped_pointer damage(new pixman_region32_t);
        pixman_region32_init(damage.data());
        return attachRender(damage.data());
    }
#else
    inline bool attachRender() {
        if (!needsAttachBuffer())
            return true;

        if (!wlr_output_attach_render(handle, nullptr))
            return false;

        attachedBuffer = true;
        return true;
    }
#endif

    inline void detachRender() {
        attachedBuffer = false;
        wlr_output_rollback(handle);
    }

    inline bool makeCurrent() {
        if (context)
            return context->makeCurrent(surface);

        return attachRender();
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
        wlr_output_set_transform(handle, static_cast<wl_output_transform>(t));
        updateProjection();
    }
    inline void setScale(float scale) {
        wlr_output_set_scale(handle, scale);
        updateProjection();
    }

    W_DECLARE_PUBLIC(WOutput)

    wlr_output *handle = nullptr;

    WBackend *backend = nullptr;
    WQuickRenderControl *rc = nullptr;
    OutputGLContext *context = nullptr;
    OutputSurface *surface = nullptr;
    WOutputLayout *layout = nullptr;
#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    WOutputDamage *damage = nullptr;
#endif
    QVector<WCursor*> cursorList;
    QScopedPointer<QCursor> cursor;

    // for software renderer
    QImage paintDevice;
    pixman_image_t *currentBuffer = nullptr;

    bool renderable = false;
#ifdef WAYLIB_DISABLE_OUTPUT_DAMAGE
    bool contentIsDirty = false;
#endif
    bool attachedBuffer = false;

    // in attached window thread
    WInputEvent *currentInputEvent = nullptr;

    QMatrix4x4 projection;
    QSurfaceFormat format;

    WSignalConnector sc;
};

class Q_DECL_HIDDEN OutputGLPlatform : public QPlatformOpenGLContext {
public:
    void initialize() override {

    }
    QSurfaceFormat format() const override {
        return static_cast<OutputGLContext*>(m_context)->output->format;
    }

    void swapBuffers(QPlatformSurface *surface) override {
        if (auto *output = OutputSurface::getOutput(surface)) {
            wlr_output_commit(output->handle);
        }
    }
    GLuint defaultFramebufferObject(QPlatformSurface *surface) const override {
        if (auto *output = OutputSurface::getOutput(surface)) {
            if (!wlr_renderer_is_gles2(output->renderer()))
                return 0;

            return wlr_gles2_renderer_get_current_fbo(output->renderer());
        }

        return 0;
    }

    bool makeCurrent(QPlatformSurface *surface) override {
        if (auto *output = OutputSurface::getOutput(surface)) {
            return output->attachRender();
        }

        return false;
    }
    void doneCurrent() override {
        if (auto *output = OutputSurface::getOutput(m_context->surface()->surfaceHandle())) {
            output->detachRender();
        }
    }

    QFunctionPointer getProcAddress(const char *procName) override {
        return eglGetProcAddress(procName);
    }

    void setContext(QOpenGLContext *context) {
        m_context = context;
    }

private:
    QOpenGLContext *m_context = nullptr;
};

class Q_DECL_HIDDEN WLRCursor : public QPlatformCursor
{
public:
    WLRCursor(WOutputPrivate *output)
        : output(output) {}

    ~WLRCursor() {

    }
    // input methods
//    void pointerEvent(const QMouseEvent & event) { Q_UNUSED(event); }
#ifndef QT_NO_CURSOR
    void changeCursor(QCursor *windowCursor, QWindow * ) override {
        if (windowCursor)
            output->setCursor(*windowCursor);
        else
            output->setCursor(WCursor::defaultCursor()); // default type
    }
    void setOverrideCursor(const QCursor &cursor) override {
        output->setCursor(cursor);
    }
    void clearOverrideCursor() override {

    }
#endif // QT_NO_CURSOR
    QPoint pos() const override {
        return QPoint();
    }
    void setPos(const QPoint &) override {
    }
    QSize size() const override {
        return QSize();
    }

private:
    WOutputPrivate *output;
};

class Q_DECL_HIDDEN OutputScreen : public QPlatformScreen
{
public:
    OutputScreen(WOutputPrivate *output)
        : output(output) {}

//    virtual bool isPlaceholder() const { return false; }

//    virtual QPixmap grabWindow(WId window, int x, int y, int width, int height) const;

    QRect geometry() const override {
        return QRect(output->q_func()->position(), output->size());
    }
//    virtual QRect availableGeometry() const {return geometry();}

    int depth() const override {
        return QImage::toPixelFormat(format()).bitsPerPixel();
    }
    QImage::Format format() const override {
        auto f = wlr_output_preferred_read_format(output->handle);
        switch (f) {
        case DRM_FORMAT_C8:
            return QImage::Format_Indexed8;
        case DRM_FORMAT_XRGB4444:
            return QImage::Format_RGB444;
        case DRM_FORMAT_ARGB4444:
            return QImage::Format_ARGB4444_Premultiplied;
        case DRM_FORMAT_XRGB1555:
            return QImage::Format_RGB555;
        case DRM_FORMAT_ARGB1555:
            return QImage::Format_ARGB8555_Premultiplied;
        case DRM_FORMAT_RGB565:
            return QImage::Format_RGB16;
        case DRM_FORMAT_RGB888:
            return QImage::Format_RGB888;
        case DRM_FORMAT_BGR888:
            return QImage::Format_BGR888;
        case DRM_FORMAT_XRGB8888:
            return QImage::Format_RGB32;
        case DRM_FORMAT_RGBX8888:
            return QImage::Format_RGBX8888;
        case DRM_FORMAT_ARGB8888:
            return QImage::Format_ARGB32_Premultiplied;
        case DRM_FORMAT_RGBA8888:
            return QImage::Format_RGBA8888;
        case DRM_FORMAT_XRGB2101010:
            return QImage::Format_RGB30;
        case DRM_FORMAT_BGRX1010102:
            return QImage::Format_BGR30;
        case DRM_FORMAT_ARGB2101010:
            return QImage::Format_A2RGB30_Premultiplied;
        case DRM_FORMAT_BGRA1010102:
            return QImage::Format_A2BGR30_Premultiplied;
        default:
            return QImage::Format_Invalid;
        }
    }

    QSizeF physicalSize() const override {
        return QSizeF(output->handle->phys_width, output->handle->phys_height);
    }
//    QDpi logicalDpi() const;
//    QDpi logicalBaseDpi() const;
    qreal devicePixelRatio() const override {
        return output->handle->scale;
    }
    qreal pixelDensity()  const override {
        return 1.0;
    }

    qreal refreshRate() const override {
        if (!output->handle->current_mode)
            return 60;
        return output->handle->current_mode->refresh;
    }

    Qt::ScreenOrientation nativeOrientation() const override {
        return output->handle->phys_width > output->handle->phys_height ?
                    Qt::LandscapeOrientation : Qt::PortraitOrientation;
    }
    Qt::ScreenOrientation orientation() const override {
        bool isPortrait = nativeOrientation() == Qt::PortraitOrientation;
        switch (output->handle->transform) {
        case WL_OUTPUT_TRANSFORM_NORMAL:
            return isPortrait ? Qt::PortraitOrientation : Qt::LandscapeOrientation;;
        case WL_OUTPUT_TRANSFORM_90:
            return isPortrait ? Qt::InvertedLandscapeOrientation : Qt::PortraitOrientation;
        case WL_OUTPUT_TRANSFORM_180:
            return isPortrait ? Qt::InvertedPortraitOrientation : Qt::InvertedLandscapeOrientation;
        case WL_OUTPUT_TRANSFORM_270:
            return isPortrait ? Qt::LandscapeOrientation : Qt::InvertedPortraitOrientation;
        default:
            break;
        }

        return Qt::PrimaryOrientation;
    }
    void setOrientationUpdateMask(Qt::ScreenOrientations) override {

    }

    QWindow *topLevelAt(const QPoint &) const override {
        return output->q_func()->attachedWindow();
    }

//    virtual QList<QPlatformScreen *> virtualSiblings() const;

    QString name() const override {
        return QString::fromUtf8(output->handle->name);
    }

    QString manufacturer() const override {
        return QString::fromUtf8(output->handle->make);
    }
    QString model() const override {
        return QString::fromUtf8(output->handle->model);
    }
    QString serialNumber() const override {
        return QString::fromUtf8(output->handle->serial);
    }

    QPlatformCursor *cursor() const override {
        if (!m_cursor)
            const_cast<OutputScreen*>(this)->m_cursor.reset(new WLRCursor(output));

        return m_cursor.get();
    }
    SubpixelAntialiasingType subpixelAntialiasingTypeHint() const override {
        switch (output->handle->subpixel) {
        case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
            return Subpixel_RGB;
        case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
            return Subpixel_BGR;
        case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
            return Subpixel_VRGB;
        case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
            return Subpixel_VBGR;
        default:
            break;
        }

        return Subpixel_None;
    }

    PowerState powerState() const override {
        return output->handle->enabled ? PowerStateOn : PowerStateOff;
    }
    void setPowerState(PowerState) override {

    }

    QVector<Mode> modes() const override {
        QVector<Mode> modes;
        struct wlr_output_mode *mode;
        wl_list_for_each(mode, &output->handle->modes, link) {
            modes << Mode {QSize(mode->width, mode->height), static_cast<qreal>(mode->refresh)};
        }

        return modes;
    }

    int currentMode() const override {
        int index = 0;
        struct wlr_output_mode *current = output->handle->current_mode;
        struct wlr_output_mode *mode;
        wl_list_for_each(mode, &output->handle->modes, link) {
            if (current == mode)
                return index;
            ++index;
        }

        return 0;
    }

    int preferredMode() const override {
        int index = 0;
        struct wlr_output_mode *mode;
        wl_list_for_each(mode, &output->handle->modes, link) {
            if (mode->preferred)
                return index;
            ++index;
        }

        return 0;
    }

private:
    WOutputPrivate *output;
    QScopedPointer<WLRCursor> m_cursor;
    friend class WOutput;
};

class Q_DECL_HIDDEN OutputWindow : public QPlatformWindow
{
public:
    OutputWindow(QWindow *w, WOutputPrivate *output)
        : QPlatformWindow(w)
        , output(output)
    {

    }

    qreal devicePixelRatio() const override {
        return output->handle->scale;
    }

private:
    WOutputPrivate *output;
    friend class WOutput;
};

QSurfaceFormat OutputSurface::Platform::format() const
{
    return static_cast<OutputSurface*>(surface())->output->format;
}

bool OutputGLContext::create()
{
    auto d = static_cast<QOpenGLContextPrivate*>(d_ptr.data());
    auto p = new OutputGLPlatform();
    d->platformGLContext = p;
    p->setContext(this);
    d->platformGLContext->initialize();
    if (!d->platformGLContext->isSharing())
        d->shareContext = nullptr;
    d->shareGroup = d->shareContext ? d->shareContext->shareGroup() : new QOpenGLContextGroup;
    d->shareGroup->d_func()->addContext(this);
    return isValid();
}

QSize OutputSurface::size() const
{
    return output->size();
}

bool OutputRenderStage::render()
{
    auto q = window->q_func();
    const int fboId = window->renderTargetId;
    const qreal devicePixelRatio = q->effectiveDevicePixelRatio();
    if (Q_UNLIKELY(fboId)) {
        QRect rect(QPoint(0, 0), window->renderTargetSize);
        window->renderer->setDeviceRect(rect);
        window->renderer->setViewportRect(rect);
        if (QQuickRenderControl::renderWindowFor(q)) {
            auto size = q->size();
            window->renderer->setProjectionMatrixToRect(QRect(QPoint(0, 0), size));
            window->renderer->setDevicePixelRatio(devicePixelRatio);
        } else {
            window->renderer->setProjectionMatrixToRect(QRect(QPoint(0, 0), rect.size()));
            window->renderer->setDevicePixelRatio(1);
        }
    } else {
        auto size = output->size();
        window->renderer->setDeviceRect(size);
        window->renderer->setViewportRect(size);
        window->renderer->setProjectionMatrix(output->projection);
        window->renderer->setDevicePixelRatio(devicePixelRatio);
    }

    if (Q_UNLIKELY(window->rhi))
        window->context->renderNextRhiFrame(window->renderer);
    else
        window->context->renderNextFrame(window->renderer, fboId);
    return true;
}

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
void WOutputPrivate::on_damage_frame(void *)
{
    renderable = true;
    doRender();
}
#else
void WOutputPrivate::on_frame(void *)
{
    renderable = true;
    doRender();
}

void WOutputPrivate::on_damage(void *)
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
    rc->window = ensureRenderWindow();

    if (wlr_renderer_is_pixman(renderer())) {
        rc->initialize(nullptr);
    } else {
        format.setRenderableType(QSurfaceFormat::OpenGLES);
        auto egl = wlr_gles2_renderer_get_egl(renderer());
        auto eglConfig = q_configFromGLFormat(egl->display, format, false, EGL_WINDOW_BIT);
        format = q_glFormatFromConfig(egl->display, eglConfig, format);

        if (!surface) {
            surface = new OutputSurface(this, QSurface::OpenGLSurface);
            surface->create();
        }

        if (!context) {
            context = new OutputGLContext(this);
            context->create();
        }

        if (!context->makeCurrent(surface)) {
            qFatal("WOutput create failed");
        }

        rc->initialize(context);
        context->doneCurrent();
    }

    q->attachedWindow()->setScreen(ensureScreen());
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

void WOutputPrivate::connect()
{
#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    sc.connect(&damage->nativeInterface<wlr_output_damage>()->events.frame,
               this, &WOutputPrivate::on_damage_frame);
#else
    sc.connect(&handle->events.frame,
               this, &WOutputPrivate::on_frame);
    sc.connect(&handle->events.damage,
               this, &WOutputPrivate::on_damage);
#endif
    // In call the connect for 'frame' signal before, maybe the wlr_output object is already
    // emit the signal, so we should suppose the renderable is true in order that ensure can
    // render on the next time
    renderable = true;

    // On the X11/Wayalnd mode，when the window size of 'output' is changed then will trigger
    // the 'mode' signal
    sc.connect(&handle->events.mode,
               this, &WOutputPrivate::on_mode);
    sc.connect(&handle->events.destroy,
               &sc, &WSignalConnector::invalidate);
}

void WOutputPrivate::updateProjection()
{
    projection = QMatrix4x4();
    projection.rotate(-90 * handle->pending.transform, 0, 0, 1);

    QSizeF size = handle->pending.transform % 2 ? QSize(handle->height, handle->width)
                                                       : this->size();

    if (handle->pending.scale > 0) {
        size /= handle->pending.scale;

        if (!paintDevice.isNull())
            paintDevice.setDevicePixelRatio(handle->pending.scale);
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

void WOutputPrivate::maybeUpdatePaintDevice()
{
    if (!wlr_renderer_is_pixman(renderer()))
        return;

    pixman_image_t *image = wlr_pixman_renderer_get_current_image(renderer());
    void *data = pixman_image_get_data(image);

    if (Q_LIKELY(paintDevice.constBits() == data))
        return;

    W_QC(WOutput);
    auto quick_window = q->attachedWindow();
    auto wd = QQuickWindowPrivate::get(quick_window);
    auto qt_renderer = dynamic_cast<QSGSoftwareRenderer*>(wd->renderer);
    Q_ASSERT(qt_renderer);

    paintDevice = WTools::fromPixmanImage(image, data);
    Q_ASSERT(!paintDevice.isNull());
    paintDevice.setDevicePixelRatio(handle->pending.scale);
    qt_renderer->setCurrentPaintDevice(&paintDevice);
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

QScreen *WOutputPrivate::ensureScreen()
{
    W_Q(WOutput);
    auto attached_window = q->attachedWindow();
    auto screen = attached_window->findChild<QScreen*>("_d_dwoutput_screen");
    if (!screen) {
        screen = new QScreen(new OutputScreen(this));
        screen->setObjectName("_d_dwoutput_screen");
        // Ensure the screen and the view window is in the same thread
        screen->moveToThread(attached_window->thread());
        screen->QObject::setParent(attached_window);
        Q_ASSERT(screen->parent() == attached_window);
    }

    return screen;
}

QWindow *WOutputPrivate::ensureRenderWindow()
{
    W_Q(WOutput);
    auto attached_window = q->attachedWindow();
    auto renderWindow = qvariant_cast<QWindow*>(attached_window->property("_d_dwoutput_render_window"));
    if (!renderWindow) {
        renderWindow = new QWindow();
        // Don't should manager the renderWindow in QGuiApplication
        QGuiApplicationPrivate::window_list.removeOne(renderWindow);
        renderWindow->setObjectName("_d_dwoutput_render_window");
        // Ensure the renderWindow and the view window is in the same thread
        renderWindow->moveToThread(attached_window->thread());
        Q_ASSERT(renderWindow->thread() == attached_window->thread());
        bool connected = QObject::connect(attached_window, &QObject::destroyed,
                                          renderWindow, &QObject::deleteLater);
        Q_ASSERT(connected);
        attached_window->setProperty("_d_dwoutput_render_window", QVariant::fromValue(renderWindow));
        auto wd = QWindowPrivate::get(renderWindow);
        wd->platformWindow = new OutputWindow(renderWindow, this);
        wd->connectToScreen(ensureScreen());
    }

    return renderWindow;
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
        rc->polishItems();
        rc->sync();

        maybeUpdatePaintDevice();
        // TODO: use scissor with the damage regions for render.
        rc->render();
    }

    if (!handle->hardware_cursor) {
        auto renderer = this->renderer();
        wlr_renderer_begin(renderer, handle->width, handle->height);
        /* Hardware cursors are rendered by the GPU on a separate plane, and can be
         * moved around without re-rendering what's beneath them - which is more
         * efficient. However, not all hardware supports hardware cursors. For this
         * reason, wlroots provides a software fallback, which we ask it to render
         * here. wlr_cursor handles configuring hardware vs software cursors for you,
         * and this function is a no-op when hardware cursors are in use. */
        wlr_output_render_software_cursors(handle, nullptr);
        wlr_renderer_end(renderer);
    }

    if (context)
        context->functions()->glFlush();

#ifndef WAYLIB_DISABLE_OUTPUT_DAMAGE
    const QSize size = transformedSize();
    enum wl_output_transform transform =
        wlr_output_transform_invert(handle->transform);

    pixman_region32_scoped_pointer frame_damage(new pixman_region32_t);
    pixman_region32_init(frame_damage.data());
    wlr_region_transform(frame_damage.data(), &this->damage->nativeInterface<wlr_output_damage>()->current,
                         transform, size.width(), size.height());
    wlr_output_set_damage(handle, frame_damage.data());
    pixman_region32_fini(frame_damage.data());
#endif

    auto dirtyFlags = handle->pending.committed;
    bool ret = wlr_output_commit(handle);
    doneCurrent();

    renderable = false;

    if (Q_LIKELY(ret)) {
        W_Q(WOutput);

        if (Q_UNLIKELY(dirtyFlags & WLR_OUTPUT_STATE_MODE)) {
            emitSizeChanged();
        }

        if (Q_UNLIKELY(dirtyFlags & WLR_OUTPUT_STATE_TRANSFORM)) {
            auto screen = ensureScreen();
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
            auto screen = ensureScreen();
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

void WOutput::attach(WQuickRenderControl *control)
{
    W_D(WOutput);

    auto thread = this->thread();
    d->rc = control;
    d->rc->prepareThread(thread);

    auto window = attachedWindow();
    Q_ASSERT(window);
#ifdef D_WM_MAIN_THREAD_QTQUICK
    Q_ASSERT(!d->inWindowThread);
#endif
    auto wd = QQuickWindowPrivate::get(window);
    Q_ASSERT(!wd->platformWindow);

    d->ensureRenderWindow();
    d->ensureScreen();

    wd->customRenderStage = new OutputRenderStage(wd, d);
#ifdef D_WM_MAIN_THREAD_QTQUICK
    d->server->threadUtils()->run(this, d, &WOutputPrivate::init);
#else
    Q_ASSERT(thread == QThread::currentThread());
    d->init();
#endif
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

        d->makeCurrent();
        d->rc->invalidate();
        d->rc = nullptr;
        d->doneCurrent();
    }

    if (d->surface) {
        delete d->surface;
        d->surface = nullptr;
    }

    if (d->context) {
        delete d->context;
        d->context = nullptr;
    }

#ifdef D_WM_MAIN_THREAD_QTQUICK
    d->inWindowThread.reset();
#endif
    d->currentBuffer = nullptr;
}

WOutputHandle *WOutput::handle() const
{
    W_DC(WOutput);
    return reinterpret_cast<WOutputHandle*>(d->handle);
}

WOutput *WOutput::fromHandle(const WOutputHandle *handle)
{
    auto wlr_handle = reinterpret_cast<const wlr_output*>(handle);
    return reinterpret_cast<WOutput*>(wlr_handle->data);
}

WOutput *WOutput::fromScreen(const QScreen *screen)
{
    return static_cast<OutputScreen*>(screen->handle())->output->q_func();
}

WOutput *WOutput::fromWindow(const QWindow *window)
{
    return static_cast<OutputWindow*>(window->handle())->output->q_func();
}

void WOutput::requestRender()
{
    W_D(WOutput);
    d->render();
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

    auto l_output = wlr_output_layout_get(d->layout->nativeInterface<wlr_output_layout>(),
                                          d->handle);

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

    return d->handle->scale;
}

QQuickWindow *WOutput::attachedWindow() const
{
    W_DC(WOutput);

    if (Q_UNLIKELY(!d->rc))
        return nullptr;
    return d->rc->d_func()->window;
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
#ifdef D_WM_MAIN_THREAD_QTQUICK
    auto object = d->ensureEventObject();

    if (Q_UNLIKELY(QThread::currentThread() == object->thread())) {
        d->processInputEvent(event);
        delete event;
        return;
    }

    QCoreApplication::postEvent(object, event);
#else
    d->processInputEvent(event);
    delete event;
#endif
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
