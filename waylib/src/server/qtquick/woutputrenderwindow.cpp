// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputrenderwindow.h"
#include "woutput.h"
#include "woutputhelper.h"
#include "wserver.h"
#include "wbackend.h"
#include "woutputviewport.h"
#include "wtools.h"
#include "wquickbackend_p.h"
#include "wwaylandcompositor_p.h"
#include "wqmlhelper_p.h"

#include "platformplugin/qwlrootsintegration.h"
#include "platformplugin/qwlrootscreen.h"
#include "platformplugin/qwlrootswindow.h"
#include "platformplugin/types.h"

#include <qwoutput.h>
#include <qwrenderer.h>
#include <qwbackend.h>
#include <qwallocator.h>
#include <qwcompositor.h>
#include <qwdamagering.h>
#include <qwbuffer.h>

#include <QOffscreenSurface>
#include <QQuickRenderControl>
#include <QOpenGLFunctions>

#define protected public
#define private public
#include <private/qsgrenderer_p.h>
#include <private/qsgsoftwarerenderer_p.h>
#undef protected
#undef private
#include <private/qquickwindow_p.h>
#include <private/qquickrendercontrol_p.h>
#include <private/qquickwindow_p.h>
#include <private/qrhi_p.h>
#include <private/qsgrhisupport_p.h>
#include <private/qquicktranslate_p.h>
#include <private/qquickitem_p.h>
#include <private/qquickanimatorcontroller_p.h>
#include <private/qsgabstractrenderer_p.h>
#include <private/qsgrenderer_p.h>
#include <private/qpainter_p.h>

extern "C" {
#define static
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/gles2.h>
#undef static
#include <wlr/render/pixman.h>
#include <wlr/render/egl.h>
#include <wlr/render/pixman.h>
#ifdef ENABLE_VULKAN_RENDER
#include <wlr/render/vulkan.h>
#endif
#include <wlr/util/region.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_damage_ring.h>
}

#include <drm_fourcc.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

struct Q_DECL_HIDDEN QScopedPointerPixmanRegion32Deleter {
    static inline void cleanup(pixman_region32_t *pointer) {
        if (pointer)
            pixman_region32_fini(pointer);
        delete pointer;
    }
};

typedef QScopedPointer<pixman_region32_t, QScopedPointerPixmanRegion32Deleter> pixman_region32_scoped_pointer;

class OutputHelper : public WOutputHelper
{
public:
    OutputHelper(WOutputViewport *output, WOutputRenderWindow *parent)
        : WOutputHelper(output->output(), parent)
        , m_output(output)
    {

    }
    ~OutputHelper()
    {

    }

    inline void init() {
        connect(this, &OutputHelper::requestRender, renderWindow(), &WOutputRenderWindow::render);
        connect(this, &OutputHelper::damaged, renderWindow(), &WOutputRenderWindow::scheduleRender);
        connect(output()->output(), &WOutput::scaleChanged, this, &OutputHelper::updateSceneDPR);
    }

    inline QWOutput *qwoutput() const {
        return output()->output()->handle();
    }

    inline WOutputRenderWindow *renderWindow() const {
        return static_cast<WOutputRenderWindow*>(parent());
    }

    WOutputViewport *output() const {
        return m_output.get();
    }

    inline QWDamageRing *damageRing() {
        return &m_damageRing;
    }

    void updateSceneDPR();

private:
    QPointer<WOutputViewport> m_output;
    QWDamageRing m_damageRing;
};

class RenderControl : public QQuickRenderControl
{
public:
    RenderControl() = default;

    QWindow *renderWindow(QPoint *) override {
        return m_renderWindow;
    }

    QWindow *m_renderWindow = nullptr;
};

class RenderContextProxy : public QSGRenderContext
{
public:
    RenderContextProxy(QSGRenderContext *target)
        : QSGRenderContext(target->sceneGraphContext())
        , target(target) {}

    bool isValid() const override {
        return target->isValid();
    }

    void initialize(const InitParams *params) override {
        return target->initialize(params);
    }
    void invalidate() override {
        target->invalidate();
    }

    void prepareSync(qreal devicePixelRatio,
                     QRhiCommandBuffer *cb,
                     const QQuickGraphicsConfiguration &config) override {
        target->prepareSync(devicePixelRatio, cb, config);
    }

    void beginNextFrame(QSGRenderer *renderer, const QSGRenderTarget &renderTarget,
                        RenderPassCallback mainPassRecordingStart,
                        RenderPassCallback mainPassRecordingEnd,
                        void *callbackUserData) override {
        target->beginNextFrame(renderer, renderTarget, mainPassRecordingStart, mainPassRecordingEnd, callbackUserData);
    }
    void renderNextFrame(QSGRenderer *renderer) override {
        renderer->setDevicePixelRatio(dpr);
        renderer->setDeviceRect(deviceRect);
        renderer->setViewportRect(viewportRect);
        renderer->setProjectionMatrix(projectionMatrix);
        renderer->setProjectionMatrixWithNativeNDC(projectionMatrixWithNativeNDC);

        target->renderNextFrame(renderer);
    }
    void endNextFrame(QSGRenderer *renderer) override {
        target->endNextFrame(renderer);
    }

    void endSync() override {
        target->endSync();
    }

    void preprocess() override {
        target->preprocess();
    }
    void invalidateGlyphCaches() override {
        target->invalidateGlyphCaches();
    }
    QSGDistanceFieldGlyphCache *distanceFieldGlyphCache(const QRawFont &font, int renderTypeQuality) override {
        return target->distanceFieldGlyphCache(font, renderTypeQuality);
    }

    QSGTexture *createTexture(const QImage &image, uint flags = CreateTexture_Alpha) const override {
        return target->createTexture(image, flags);
    }
    QSGRenderer *createRenderer(QSGRendererInterface::RenderMode renderMode = QSGRendererInterface::RenderMode2D) override {
        return target->createRenderer(renderMode);
    }
    QSGTexture *compressedTextureForFactory(const QSGCompressedTextureFactory *tf) const override {
        return target->compressedTextureForFactory(tf);
    }

    int maxTextureSize() const override {
        return target->maxTextureSize();
    }

    QRhi *rhi() const override {
        return target->rhi();
    }

    QSGRenderContext *target;
    qreal dpr;
    QRect deviceRect;
    QRect viewportRect;
    QMatrix4x4 projectionMatrix;
    QMatrix4x4 projectionMatrixWithNativeNDC;
};

static QEvent::Type doRenderEventType = static_cast<QEvent::Type>(QEvent::registerEventType());
class WOutputRenderWindowPrivate : public QQuickWindowPrivate
{
public:
    WOutputRenderWindowPrivate(WOutputRenderWindow *)
        : QQuickWindowPrivate() {
    }

    static inline WOutputRenderWindowPrivate *get(WOutputRenderWindow *qq) {
        return qq->d_func();
    }

    inline RenderControl *rc() const {
        return static_cast<RenderControl*>(q_func()->renderControl());
    }

    inline bool isInitialized() const {
        return renderContextProxy.get();
    }

    inline void setSceneDevicePixelRatio(qreal ratio) {
        static_cast<QWlrootsRenderWindow*>(platformWindow)->setDevicePixelRatio(ratio);
    }

    inline bool isComponentComplete() const {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        return componentComplete;
#else
        return componentCompleted;
#endif
    }

    QSGRendererInterface::GraphicsApi graphicsApi() const;
    void init();
    void init(OutputHelper *helper);
    bool initRCWithRhi();
    void updateSceneDPR();

    void doRender();
    inline void scheduleDoRender() {
        if (!isInitialized())
            return; // Not initialized

        QCoreApplication::postEvent(q_func(), new QEvent(doRenderEventType));
    }

    Q_DECLARE_PUBLIC(WOutputRenderWindow)

    WWaylandCompositor *compositor = nullptr;

    QList<OutputHelper*> outputs;

    std::unique_ptr<RenderContextProxy> renderContextProxy;
    QOpenGLContext *glContext = nullptr;
#ifdef ENABLE_VULKAN_RENDER
    QScopedPointer<QVulkanInstance> vkInstance;
#endif
};

void OutputHelper::updateSceneDPR()
{
    WOutputRenderWindowPrivate::get(renderWindow())->updateSceneDPR();
}

QSGRendererInterface::GraphicsApi WOutputRenderWindowPrivate::graphicsApi() const
{
    auto api = WOutputHelper::getGraphicsApi(rc());
    Q_ASSERT(api == WOutputHelper::getGraphicsApi());
    return api;
}

void WOutputRenderWindowPrivate::init()
{
    Q_ASSERT(compositor);
    Q_Q(WOutputRenderWindow);

    if (QSGRendererInterface::isApiRhiBased(graphicsApi()))
        initRCWithRhi();
    Q_ASSERT(context);
    renderContextProxy.reset(new RenderContextProxy(context));
    q->create();
    rc()->m_renderWindow = q;

    // Configure the QSGRenderer at QSGRenderContext::renderNextFrame
    QObject::connect(q, &WOutputRenderWindow::beforeRendering, q, [this] {
        context = renderContextProxy.get();
    }, Qt::DirectConnection);
    QObject::connect(q, &WOutputRenderWindow::afterRendering, q, [this] {
        context = renderContextProxy->target;
    }, Qt::DirectConnection);

    for (auto output : outputs)
        init(output);
    updateSceneDPR();

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
    // TODO: Get damage regions from the Qt, and use WOutputDamage::add instead of WOutput::update.
    QObject::connect(rc(), &QQuickRenderControl::renderRequested,
                     q, &WOutputRenderWindow::update);
    QObject::connect(rc(), &QQuickRenderControl::sceneChanged,
                     q, &WOutputRenderWindow::update);
}

void WOutputRenderWindowPrivate::init(OutputHelper *helper)
{
    W_Q(WOutputRenderWindow);
    QMetaObject::invokeMethod(q, &WOutputRenderWindow::scheduleRender, Qt::QueuedConnection);
    helper->init();
}

inline static QByteArrayList fromCStyleList(size_t count, const char **list) {
    QByteArrayList al;
    al.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        al.append(list[i]);
    }
    return al;
}

bool WOutputRenderWindowPrivate::initRCWithRhi()
{
    W_Q(WOutputRenderWindow);

    QQuickRenderControlPrivate *rcd = QQuickRenderControlPrivate::get(rc());
    QSGRhiSupport *rhiSupport = QSGRhiSupport::instance();

// sanity check for Vulkan
#ifdef ENABLE_VULKAN_RENDER
    if (rhiSupport->rhiBackend() == QRhi::Vulkan) {
        vkInstance.reset(new QVulkanInstance());

        auto phdev = wlr_vk_renderer_get_physical_device(compositor->renderer()->handle());
        auto dev = wlr_vk_renderer_get_device(compositor->renderer()->handle());
        auto queue_family = wlr_vk_renderer_get_queue_family(compositor->renderer()->handle());

#if QT_VERSION > QT_VERSION_CHECK(6, 6, 0)
        auto instance = wlr_vk_renderer_get_instance(compositor->renderer()->handle());
        vkInstance->setVkInstance(instance);
#endif
        //        vkInstance->setExtensions(fromCStyleList(vkRendererAttribs.extension_count, vkRendererAttribs.extensions));
        //        vkInstance->setLayers(fromCStyleList(vkRendererAttribs.layer_count, vkRendererAttribs.layers));
        vkInstance->setApiVersion({1, 1, 0});
        vkInstance->create();
        q->setVulkanInstance(vkInstance.data());

        auto gd = QQuickGraphicsDevice::fromDeviceObjects(phdev, dev, queue_family);
        q->setGraphicsDevice(gd);
    } else
#endif
    if (rhiSupport->rhiBackend() == QRhi::OpenGLES2) {
        Q_ASSERT(wlr_renderer_is_gles2(compositor->renderer()->handle()));
        auto egl = wlr_gles2_renderer_get_egl(compositor->renderer()->handle());
        auto display = wlr_egl_get_display(egl);
        auto context = wlr_egl_get_context(egl);

        this->glContext = new QW::OpenGLContext(display, context, rc());
        bool ok = this->glContext->create();
        if (!ok)
            return false;

        q->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(this->glContext));
    } else {
        return false;
    }

    QOffscreenSurface *offscreenSurface = new QW::OffscreenSurface(nullptr, q);
    offscreenSurface->create();

    QSGRhiSupport::RhiCreateResult result = rhiSupport->createRhi(q, offscreenSurface);
    if (!result.rhi) {
        qWarning("WOutput::initRhi: Failed to initialize QRhi");
        return false;
    }

    rcd->rhi = result.rhi;
    // Ensure the QQuickRenderControl don't reinit the RHI
    rcd->ownRhi = true;
    if (!rc()->initialize())
        return false;
    rcd->ownRhi = result.own;
    Q_ASSERT(rcd->rhi == result.rhi);

    return true;
}

void WOutputRenderWindowPrivate::updateSceneDPR()
{
    if (outputs.isEmpty())
        return;

    qreal maxDPR = 0.0;

    for (auto o : outputs) {
        if (o->output()->output()->scale() > maxDPR)
            maxDPR = o->output()->output()->scale();
    }

    setSceneDevicePixelRatio(maxDPR);
}

inline static WImageRenderTarget *getImageFrom(const QQuickRenderTarget &rt)
{
    auto d = QQuickRenderTargetPrivate::get(&rt);
    Q_ASSERT(d->type == QQuickRenderTargetPrivate::Type::PaintDevice);
    return static_cast<WImageRenderTarget*>(d->u.paintDevice);
}

struct PixmanRegion
{
    PixmanRegion() {
        pixman_region32_init(&data);
    }
    ~PixmanRegion() {
        pixman_region32_fini(&data);
    }

    inline operator pixman_region32_t*() {
        return &data;
    }

    inline bool isEmpty() const {
        return !pixman_region32_not_empty(&data);
    }

    pixman_region32_t data;
};

void WOutputRenderWindowPrivate::doRender()
{
    bool needPolishItems = true;
    for (OutputHelper *helper : outputs) {
        if (!helper->renderable() || !helper->output()->isVisible())
            continue;

        if (needPolishItems) {
            rc()->polishItems();
            needPolishItems = false;
        }

        if (!helper->contentIsDirty()) {
            if (helper->needsFrame()) {
                if (helper->qwoutput()->commit())
                    helper->resetState();
            }
            continue;
        }

        const auto lastRT = helper->lastRenderTarget();
        int bufferAge = 0;
        auto rt = helper->acquireRenderTarget(rc(), &bufferAge);
        Q_ASSERT(rt.first);
        if (rt.second.isNull())
            continue;

        if (Q_UNLIKELY(!helper->makeCurrent(rt.first, glContext)))
            continue;

        {
            q_func()->setRenderTarget(rt.second);
            // TODO: new render thread
            if (QSGRendererInterface::isApiRhiBased(WOutputHelper::getGraphicsApi()))
                rc()->beginFrame();
            rc()->sync();

            Q_ASSERT(helper->output()->output()->scale() <= q_func()->devicePixelRatio());
            const qreal devicePixelRatio = helper->output()->devicePixelRatio();
            const QSize pixelSize = helper->output()->output()->size();

            auto viewportMatrix = QQuickItemPrivate::get(helper->output())->itemNode()->matrix().inverted();
            QMatrix4x4 parentMatrix = QQuickItemPrivate::get(helper->output()->parentItem())->itemToWindowTransform().inverted();
            viewportMatrix *= parentMatrix;

            auto softwareRenderer = dynamic_cast<QSGSoftwareRenderer*>(this->renderer);
            if (softwareRenderer) {
                auto image = getImageFrom(rt.second);
                image->setDevicePixelRatio(devicePixelRatio);
                auto rootTransformNode = QQuickItemPrivate::get(contentItem)->itemNode();
                // TODO: Should set to QSGSoftwareRenderer, but it's not support specify matrix.
                if (rootTransformNode->matrix() != viewportMatrix)
                    rootTransformNode->setMatrix(viewportMatrix);
            } else {
                bool flipY = rhi ? !rhi->isYUpInNDC() : false;
                if (!customRenderTarget.isNull() && customRenderTarget.mirrorVertically())
                    flipY = !flipY;

                renderContextProxy->dpr = devicePixelRatio;
                renderContextProxy->deviceRect = QRect(QPoint(0, 0), pixelSize);
                renderContextProxy->viewportRect = QRect(QPoint(0, 0), pixelSize);

                QRectF rect(QPointF(0, 0), helper->output()->size());

                const float left = rect.x();
                const float right = rect.x() + rect.width();
                float bottom = rect.y() + rect.height();
                float top = rect.y();

                if (flipY)
                    std::swap(top, bottom);

                QMatrix4x4 matrix;
                matrix.ortho(left, right, bottom, top, 1, -1);
                renderContextProxy->projectionMatrix = matrix * viewportMatrix;

                if (rhi && !rhi->isYUpInNDC()) {
                    std::swap(top, bottom);

                    matrix.setToIdentity();
                    matrix.ortho(left, right, bottom, top, 1, -1);
                }
                renderContextProxy->projectionMatrixWithNativeNDC = matrix * viewportMatrix;
            }

            // TODO: use scissor with the damage regions for render.
            rc()->render();

            if (softwareRenderer) {
                auto currentImage = getImageFrom(rt.second);
                Q_ASSERT(currentImage && currentImage == softwareRenderer->m_rt.paintDevice);
                currentImage->setDevicePixelRatio(1.0);
                const auto scaleTF = QTransform::fromScale(devicePixelRatio, devicePixelRatio);
                const auto scaledFlushRegion = scaleTF.map(softwareRenderer->flushRegion());
                PixmanRegion scaledFlushDamage;
                bool ok = WTools::toPixmanRegion(scaledFlushRegion, scaledFlushDamage);
                Q_ASSERT(ok);

                {
                    PixmanRegion damage;
                    // TODO: Use QWDamageRing::setBounds
                    helper->damageRing()->setBounds(pixelSize);
                    helper->damageRing()->getBufferDamage(bufferAge, damage);

                    if (!damage.isEmpty() && lastRT.first != rt.first && !lastRT.second.isNull()) {
                        auto image = getImageFrom(lastRT.second);
                        Q_ASSERT(image);
                        Q_ASSERT(image->size() == pixelSize);

                        // TODO: Don't use the previous render target, we can get the damage region of QtQuick
                        // before QQuickRenderControl::render for QWDamageRing, and add dirty region to
                        // QSGAbstractSoftwareRenderer to force repaint the damage region of current render target.
                        QPainter pa(currentImage);

                        PixmanRegion remainderDamage;
                        ok = pixman_region32_subtract(remainderDamage, damage, scaledFlushDamage);
                        Q_ASSERT(ok);

                        int count = 0;
                        auto rects = pixman_region32_rectangles(remainderDamage, &count);
                        for (int i = 0; i < count; ++i) {
                            auto r = rects[i];
                            pa.drawImage(r.x1, r.y1, *image, r.x1, r.y1, r.x2 - r.x1, r.y2 - r.y1);
                        }
                    }
                }

                helper->damageRing()->add(scaledFlushDamage);
                if (!softwareRenderer->flushRegion().isEmpty())
                    helper->qwoutput()->setDamage(&helper->damageRing()->handle()->current);
            }

            if (QSGRendererInterface::isApiRhiBased(WOutputHelper::getGraphicsApi()))
                rc()->endFrame();
        }

        if (helper->qwoutput()->commit())
            helper->resetState();
        helper->doneCurrent(glContext);
        helper->damageRing()->rotate();

        Q_EMIT helper->output()->frameDone();
    }
}

// TODO: Support QWindow::setCursor
WOutputRenderWindow::WOutputRenderWindow(QObject *parent)
    : QQuickWindow(*new WOutputRenderWindowPrivate(this), new RenderControl())
{
    setObjectName(QW::RenderWindow::id());

    if (parent)
        QObject::setParent(parent);
}

WOutputRenderWindow::~WOutputRenderWindow()
{
    renderControl()->disconnect(this);
    renderControl()->invalidate();
    renderControl()->deleteLater();
}

QQuickRenderControl *WOutputRenderWindow::renderControl() const
{
    auto wd = QQuickWindowPrivate::get(this);
    return wd->renderControl;
}

void WOutputRenderWindow::attach(WOutputViewport *output)
{
    Q_D(WOutputRenderWindow);

    Q_ASSERT(std::find_if(d->outputs.cbegin(), d->outputs.cend(), [output] (OutputHelper *h) {
                 return h->output() == output;
    }) == d->outputs.cend());

    Q_ASSERT(output->output());

    d->outputs << new OutputHelper(output, this);
    if (d->compositor) {
        auto qwoutput = d->outputs.last()->qwoutput();
        qwoutput->initRender(d->compositor->allocator(), d->compositor->renderer());
    }

    if (!d->isInitialized())
        return;

    d->updateSceneDPR();
    d->init(d->outputs.last());
    d->scheduleDoRender();
}

void WOutputRenderWindow::detach(WOutputViewport *output)
{
    Q_D(WOutputRenderWindow);

    OutputHelper *helper = nullptr;
    for (int i = 0; i < d->outputs.size(); ++i) {
        if (d->outputs.at(i)->output() == output) {
            helper = d->outputs.at(i);
            d->outputs.removeAt(i);
            break;
        }
    }
    Q_ASSERT(helper);
    helper->deleteLater();

    d->updateSceneDPR();
}

WWaylandCompositor *WOutputRenderWindow::compositor() const
{
    Q_D(const WOutputRenderWindow);
    return d->compositor;
}

void WOutputRenderWindow::setCompositor(WWaylandCompositor *newCompositor)
{
    Q_D(WOutputRenderWindow);
    Q_ASSERT(!d->compositor);
    d->compositor = newCompositor;

    for (auto output : d->outputs) {
        auto qwoutput = output->qwoutput();
        qwoutput->initRender(d->compositor->allocator(), d->compositor->renderer());
    }

    if (d->isComponentComplete() && d->compositor->isPolished()) {
        d->init();
    } else {
        connect(newCompositor, &WWaylandCompositor::afterPolish, this, [d] {
            if (d->isComponentComplete())
                d->init();
        });
    }
}

void WOutputRenderWindow::render()
{
    Q_D(WOutputRenderWindow);
    d->doRender();
}

void WOutputRenderWindow::scheduleRender()
{
    Q_D(WOutputRenderWindow);
    d->scheduleDoRender();
}

void WOutputRenderWindow::update()
{
    Q_D(WOutputRenderWindow);
    for (auto o : d->outputs)
        o->update(); // make contents to dirty
    d->scheduleDoRender();
}

void WOutputRenderWindow::classBegin()
{
    Q_D(WOutputRenderWindow);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    d->componentComplete = false;
#else
    d->componentCompleted = false;
#endif
}

void WOutputRenderWindow::componentComplete()
{
    Q_D(WOutputRenderWindow);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    d->componentComplete = true;
#else
    d->componentCompleted = true;
#endif

    if (d->compositor && d->compositor->isPolished())
        d->init();
}

bool WOutputRenderWindow::event(QEvent *event)
{
    Q_D(WOutputRenderWindow);

    if (event->type() == doRenderEventType) {
        d_func()->doRender();
        return true;
    }

    if (QW::RenderWindow::beforeDisposeEventFilter(this, event)) {
        event->accept();
        QW::RenderWindow::afterDisposeEventFilter(this, event);
        return true;
    }

    bool isAccepted = QQuickWindow::event(event);
    if (QW::RenderWindow::afterDisposeEventFilter(this, event))
        return true;

    return isAccepted;
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputrenderwindow.cpp"
