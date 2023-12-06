// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wbufferrenderer_p.h"
#include "wtexture.h"
#include "wrenderhelper.h"
#include "wqmlhelper_p.h"
#include "wtools.h"

#include <qwbuffer.h>
#include <qwtexture.h>
#include <qwrenderer.h>
#include <qwswapchain.h>
#include <qwoutput.h>

#include <QSGImageNode>

#define protected public
#define private public
#include <private/qsgsoftwarerenderer_p.h>
#include <private/qsgsoftwarerenderablenodeupdater_p.h>
#include <private/qsgsoftwarerenderablenode_p.h>
#undef protected
#undef private
#include <private/qsgplaintexture_p.h>
#include <private/qquickitem_p.h>
#include <private/qsgdefaultrendercontext_p.h>
#include <private/qsgrenderer_p.h>
#include <private/qrhi_p.h>
#ifndef QT_NO_OPENGL
#include <private/qrhigles2_p.h>
#include <private/qopenglcontext_p.h>
#endif

extern "C" {
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/render/drm_format_set.h>
#define static
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/interface.h>
#undef static
#ifdef slots
#undef slots // using at swapchain.h
#endif
#include <wlr/render/swapchain.h>
}

#include <pixman.h>
#include <drm_fourcc.h>
#include <xf86drm.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

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

inline static WImageRenderTarget *getImageFrom(const QQuickRenderTarget &rt)
{
    auto d = QQuickRenderTargetPrivate::get(&rt);
    Q_ASSERT(d->type == QQuickRenderTargetPrivate::Type::PaintDevice);
    return static_cast<WImageRenderTarget*>(d->u.paintDevice);
}

static const wlr_drm_format *pickFormat(QWRenderer *renderer, uint32_t format)
{
    auto r = renderer->handle();
    if (!r->impl->get_render_formats) {
        return nullptr;
    }
    const wlr_drm_format_set *format_set = r->impl->get_render_formats(r);
    if (!format_set)
        return nullptr;

    return wlr_drm_format_set_get(format_set, format);
}

static QSGRootNode *getRootNode(QQuickItem *item)
{
    const auto d = QQuickItemPrivate::get(item);
    QSGNode *root = d->itemNode();
    if (!root)
        return nullptr;

    while (root->firstChild() && root->type() != QSGNode::RootNodeType)
        root = root->firstChild();
    return root->type() == QSGNode::RootNodeType ? static_cast<QSGRootNode*>(root) : nullptr;
}

static void applyTransform(QSGSoftwareRenderer *renderer, const QTransform &t)
{
    if (t.isIdentity())
        return;

    auto nodeIter = renderer->m_nodes.begin();
    while (nodeIter != renderer->m_nodes.end()) {
        auto node = *nodeIter;
        node->setTransform(node->transform() * t);

        if (node->m_hasClipRegion)
            node->setClipRegion(t.map(node->clipRegion()), true);

        ++nodeIter;
    }
}

class TextureProvider : public QSGTextureProvider
{
public:
    explicit TextureProvider(WBufferRenderer *item)
        : item(item) {}
    ~TextureProvider() {

    }

    QSGTexture *texture() const override {
        return dwtexture ? dwtexture->getSGTexture(item->window()) : nullptr;
    }

    void setBuffer(QWBuffer *buffer) {
        qwtexture.reset();

        if (Q_LIKELY(buffer)) {
            qwtexture.reset(QWTexture::fromBuffer(item->output()->renderer(), buffer));
        }

        if (Q_LIKELY(qwtexture)) {
            if (Q_LIKELY(dwtexture)) {
                dwtexture->setHandle(qwtexture.get());
            } else {
                dwtexture.reset(new WTexture(qwtexture.get()));
            }
        } else {
            dwtexture.reset();
        }

        Q_EMIT textureChanged();
    }

    WBufferRenderer *item;
    std::unique_ptr<QWTexture> qwtexture;
    std::unique_ptr<WTexture> dwtexture;
};

WBufferRenderer::WBufferRenderer(QQuickItem *parent)
    : QQuickItem(parent)
    , m_cacheBuffer(false)
    , m_forceCacheBuffer(false)
    , m_hideSource(false)
{

}

WBufferRenderer::~WBufferRenderer()
{
    resetSource();

    m_renderer->deleteLater();
    delete m_renderHelper;
    delete m_swapchain;
}

WOutput *WBufferRenderer::output() const
{
    return m_output;
}

void WBufferRenderer::setOutput(WOutput *output)
{
    if (m_output == output)
        return;
    m_output = output;
    Q_EMIT sceneGraphChanged();
}

QQuickItem *WBufferRenderer::source() const
{
    return m_source;
}

void WBufferRenderer::setSource(QQuickItem *s, bool hideSource)
{
    if (m_source == s)
        return;

    resetSource();
    m_source = s;
    m_hideSource = hideSource;

    if (m_source) {
        connect(m_source, &QQuickItem::destroyed, this, [this] {
            m_source = nullptr;
            m_rootNode = nullptr;
        });
        auto d = QQuickItemPrivate::get(m_source);
        d->refFromEffectItem(m_hideSource);
    }

    Q_EMIT sceneGraphChanged();
}

bool WBufferRenderer::cacheBuffer() const
{
    return m_cacheBuffer;
}

void WBufferRenderer::setCacheBuffer(bool newCacheBuffer)
{
    if (m_cacheBuffer == newCacheBuffer)
        return;
    m_cacheBuffer = newCacheBuffer;
    updateTextureProvider();

    Q_EMIT cacheBufferChanged();
}

QWBuffer *WBufferRenderer::lastBuffer() const
{
    return m_lastBuffer;
}

const QWDamageRing *WBufferRenderer::damageRing() const
{
    return &m_damageRing;
}

QWDamageRing *WBufferRenderer::damageRing()
{
    return &m_damageRing;
}

bool WBufferRenderer::isTextureProvider() const
{
    return m_cacheBuffer;
}

QSGTextureProvider *WBufferRenderer::textureProvider() const
{
    return m_textureProvider.get();
}

QWBuffer *WBufferRenderer::render(QSGRenderContext *context, uint32_t format,
                                  const QSize &pixelSize, qreal devicePixelRatio,
                                  QMatrix4x4 renderMatrix, RenderFlags flags)
{
    m_lastBuffer.clear();
    Q_ASSERT(m_output);

    auto dr = qobject_cast<QSGDefaultRenderContext*>(context);
    auto wd = QQuickWindowPrivate::get(window());
    QSGRenderer *renderer = nullptr;

    if (m_source) {
        if (!m_renderer) {
            m_rootNode = getRootNode(m_source);
            Q_ASSERT(m_rootNode);

            const bool useDepth = dr ? dr->useDepthBufferFor2D() : false;
            const auto renderMode = useDepth ? QSGRendererInterface::RenderMode2D
                                             : QSGRendererInterface::RenderMode2DNoDepthBuffer;
            m_renderer = context->createRenderer(renderMode);
            m_renderer->setRootNode(m_rootNode);
            QObject::connect(m_renderer, &QSGRenderer::sceneGraphChanged,
                             this, &WBufferRenderer::sceneGraphChanged);
        }

        m_renderer->setClearColor(Qt::transparent);
        renderer = m_renderer;
    } else {
        renderer = wd->renderer;
    }

    m_damageRing.setBounds(pixelSize);

    renderer->setDevicePixelRatio(devicePixelRatio);
    renderer->setDeviceRect(QRect(QPoint(0, 0), pixelSize));
    renderer->setViewportRect(pixelSize);

    if (!m_renderHelper)
        m_renderHelper = new WRenderHelper(m_output->renderer());
    m_renderHelper->setSize(pixelSize);

    // configure swapchain
    if (flags.testFlag(RenderFlag::DontConfigureSwapchain)) {
        auto renderFormat = pickFormat(m_output->renderer(), format);
        if (!renderFormat) {
            qWarning("wlr_renderer doesn't support format 0x%s", drmGetFormatName(format));
            return nullptr;
        }
        if (!m_swapchain || QSize(m_swapchain->handle()->width, m_swapchain->handle()->height) != pixelSize
            || m_swapchain->handle()->format.format != renderFormat->format) {
            if (m_swapchain)
                delete m_swapchain;
            m_swapchain = QWSwapchain::create(m_output->allocator(), pixelSize, renderFormat);
        }
    } else {
        bool ok = m_output->configureSwapchain(pixelSize, format, &m_swapchain, flags);
        if (!ok)
            return nullptr;
    }

    // TODO: Support scanout buffer of wlr_surface(from WSurfaceItem)
    int bufferAge;
    auto buffer = m_swapchain->acquire(&bufferAge);
    if (!buffer)
        return nullptr;

    Q_ASSERT(wd->renderControl);
    auto lastRT = m_renderHelper->lastRenderTarget();
    auto rt = m_renderHelper->acquireRenderTarget(wd->renderControl, buffer);
    if (rt.isNull()) {
        buffer->unlock();
        return nullptr;
    }

    auto rtd = QQuickRenderTargetPrivate::get(&rt);
    QSGRenderTarget sgRT;

    if (rtd->type == QQuickRenderTargetPrivate::Type::PaintDevice) {
        sgRT.paintDevice = rtd->u.paintDevice;
    } else {
        Q_ASSERT(rtd->type == QQuickRenderTargetPrivate::Type::RhiRenderTarget);
        sgRT.rt = rtd->u.rhiRt;
        sgRT.cb = wd->redirect.commandBuffer;
        Q_ASSERT(sgRT.cb);
        sgRT.rpDesc = rtd->u.rhiRt->renderPassDescriptor();
    }
    renderer->setRenderTarget(sgRT);

    auto softwareRenderer = dynamic_cast<QSGSoftwareRenderer*>(renderer);
    { // before render
        if (softwareRenderer) {
            auto image = getImageFrom(rt);
            image->setDevicePixelRatio(devicePixelRatio);

            // TODO: Should set to QSGSoftwareRenderer, but it's not support specify matrix.
            // If transform is changed, it will full repaint.
            if (m_source) {
                auto t = renderMatrix.toTransform();
                if (t.type() > QTransform::TxTranslate) {
                    (image->operator QImage &()).fill(renderer->clearColor());
                    softwareRenderer->markDirty();
                }

                applyTransform(softwareRenderer, t);
            } else {
                auto rootTransform = QQuickItemPrivate::get(wd->contentItem)->itemNode();
                if (rootTransform->matrix() != renderMatrix)
                    rootTransform->setMatrix(renderMatrix);
            }
        } else {
            bool flipY = wd->rhi ? !wd->rhi->isYUpInNDC() : false;
            if (!rt.isNull() && rt.mirrorVertically())
                flipY = !flipY;

            QRectF rect(QPointF(0, 0), QSizeF(pixelSize) / devicePixelRatio);

            const float left = rect.x();
            const float right = rect.x() + rect.width();
            float bottom = rect.y() + rect.height();
            float top = rect.y();

            if (flipY)
                std::swap(top, bottom);

            QMatrix4x4 matrix;
            matrix.ortho(left, right, bottom, top, 1, -1);

            QMatrix4x4 projectionMatrix, projectionMatrixWithNativeNDC;
            projectionMatrix = matrix * renderMatrix;

            if (wd->rhi && !wd->rhi->isYUpInNDC()) {
                std::swap(top, bottom);

                matrix.setToIdentity();
                matrix.ortho(left, right, bottom, top, 1, -1);
            }
            projectionMatrixWithNativeNDC = matrix * renderMatrix;

            renderer->setProjectionMatrix(projectionMatrix);
            renderer->setProjectionMatrixWithNativeNDC(projectionMatrixWithNativeNDC);

#ifndef QT_NO_OPENGL
            if (!m_source && wd->rhi->backend() == QRhi::OpenGLES2) {
                auto glRT = QRHI_RES(QGles2TextureRenderTarget, rtd->u.rhiRt);
                Q_ASSERT(glRT->framebuffer > 0);
                auto glContext = QOpenGLContext::currentContext();
                Q_ASSERT(glContext);
                QOpenGLContextPrivate::get(glContext)->defaultFboRedirect = glRT->framebuffer;
            }
#endif
        }
    }

    context->renderNextFrame(renderer);

    { // after render
        if (!softwareRenderer) {
            // TODO: get damage area from QRhi renderer
            m_damageRing.addWhole();

#ifndef QT_NO_OPENGL
            if (!m_source && wd->rhi->backend() == QRhi::OpenGLES2) {
                auto glContext = QOpenGLContext::currentContext();
                Q_ASSERT(glContext);
                QOpenGLContextPrivate::get(glContext)->defaultFboRedirect = GL_NONE;
            }
#endif
        } else {
            auto currentImage = getImageFrom(rt);
            Q_ASSERT(currentImage && currentImage == softwareRenderer->m_rt.paintDevice);
            currentImage->setDevicePixelRatio(1.0);
            const auto scaleTF = QTransform::fromScale(devicePixelRatio, devicePixelRatio);
            const auto scaledFlushRegion = scaleTF.map(softwareRenderer->flushRegion());
            PixmanRegion scaledFlushDamage;
            bool ok = WTools::toPixmanRegion(scaledFlushRegion, scaledFlushDamage);
            Q_ASSERT(ok);

            {
                PixmanRegion damage;
                m_damageRing.getBufferDamage(bufferAge, damage);

                if (!damage.isEmpty() && lastRT.first != buffer && !lastRT.second.isNull()) {
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

            if (m_source)
                applyTransform(softwareRenderer, renderMatrix.inverted().toTransform());
            m_damageRing.add(scaledFlushDamage);
        }
    }

    if (flags.testFlag(UpdateResource) && dr) {
        QRhiResourceUpdateBatch *resourceUpdates = wd->rhi->nextResourceUpdateBatch();
        dr->currentFrameCommandBuffer()->resourceUpdate(resourceUpdates);
    }

    if (m_textureProvider.get())
        m_textureProvider->setBuffer(buffer);

    m_lastBuffer = buffer;

    return buffer;
}

void WBufferRenderer::setBufferSubmitted(QWLRoots::QWBuffer *buffer)
{
    m_damageRing.rotate();
    m_swapchain->setBufferSubmitted(buffer);
}

void WBufferRenderer::componentComplete()
{
    QQuickItem::componentComplete();
}

void WBufferRenderer::ensureTextureProvider()
{
    m_forceCacheBuffer = true;
    updateTextureProvider();
}

void WBufferRenderer::resetTextureProvider()
{
    m_forceCacheBuffer = false;
    updateTextureProvider();
}

void WBufferRenderer::updateTextureProvider()
{
    if (shouldCacheBuffer()) {
        if (!m_textureProvider.get())
            m_textureProvider.reset(new TextureProvider(this));
        if (m_lastBuffer)
            m_textureProvider->setBuffer(m_lastBuffer);
    } else if (m_textureProvider.get()) {
        m_textureProvider.reset();
    }

    if (flags().testFlag(ItemHasContents))
        update();
}

QSGNode *WBufferRenderer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (!m_textureProvider.get()) {
        delete oldNode;
        return nullptr;
    }

    auto node = static_cast<QSGImageNode*>(oldNode);
    if (Q_UNLIKELY(!node)) {
        node = window()->createImageNode();
        node->setOwnsTexture(false);
        node->setTexture(m_textureProvider->texture());
    } else {
        node->markDirty(QSGNode::DirtyMaterial);
    }

    const QRectF textureGeometry = QRectF(QPointF(0, 0), node->texture()->textureSize());
    node->setSourceRect(textureGeometry);
    const QRectF targetGeometry(QPointF(0, 0), size());
    node->setRect(targetGeometry);
    node->setFiltering(QSGTexture::Linear);
    node->setMipmapFiltering(QSGTexture::None);

    return node;
}

void WBufferRenderer::invalidateSceneGraph()
{
    if (m_textureProvider)
        m_textureProvider.reset();
}

void WBufferRenderer::releaseResources()
{
    if (m_textureProvider) {
        class TextureProviderCleanupJob : public QRunnable
        {
        public:
            TextureProviderCleanupJob(QObject *object) : m_object(object) { }
            void run() override {
                delete m_object;
            }
            QObject *m_object;
        };

        // Delay clean the textures on the next render after.
        window()->scheduleRenderJob(new TextureProviderCleanupJob(m_textureProvider.release()),
                                    QQuickWindow::AfterRenderingStage);
    }
}

void WBufferRenderer::resetSource()
{
    if (m_source) {
        auto d = QQuickItemPrivate::get(m_source);
        d->derefFromEffectItem(m_hideSource);
        m_source = nullptr;
    }
    m_rootNode = nullptr;
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wbufferrenderer_p.cpp"
