// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wbufferrenderer_p.h"
#include "wayliblogging.h"
#include "wrenderhelper.h"
#include "wqmlhelper_p.h"
#include "woutputviewport_p.h"
#include "wtools.h"
#include "wsgtextureprovider.h"
#include "private/wprivateaccessor_p.h"
#include "utils/private/wvulkantrace_p.h"

#include <qwbuffer.h>
#include <qwtexture.h>
#include <qwrenderer.h>
#include <qwswapchain.h>
#include <qwoutput.h>
#include <qwallocator.h>
#include <qwrendererinterface.h>

#include <QSGImageNode>
#include <QSGSimpleRectNode>
#include <QSet>

#include <private/qsgsoftwarerenderer_p.h>
#include <private/qsgsoftwarerenderablenodeupdater_p.h>
#include <private/qsgsoftwarerenderablenode_p.h>
#include <private/qsgplaintexture_p.h>
#include <private/qquickitem_p.h>
#include <private/qsgdefaultrendercontext_p.h>
#include <private/qsgrenderer_p.h>
#include <private/qrhi_p.h>
#ifndef QT_NO_OPENGL
#include <private/qrhigles2_p.h>
#include <private/qopenglcontext_p.h>
#endif
#include <private/qsgbatchrenderer_p.h>

#include <pixman.h>
#include <drm_fourcc.h>
#include <xf86drm.h>

using QSGAbsSoftRenderer_NodesMap = QHash<QSGNode*, QSGSoftwareRenderableNode*>;
W_DECLARE_PRIVATE_MEMBER(QSGAbsSoftRenderer_m_nodes_tag, QSGAbstractSoftwareRenderer, m_nodes, QSGAbsSoftRenderer_NodesMap);
W_DECLARE_PRIVATE_MEMBER(QSGAbsSoftRenderer_m_background_tag, QSGAbstractSoftwareRenderer, m_background, QSGSimpleRectNode*);
W_DECLARE_PRIVATE_MEMBER(QSGAbsSoftRenderer_m_dirtyRegion_tag, QSGAbstractSoftwareRenderer, m_dirtyRegion, QRegion);
W_DECLARE_PRIVATE_MEMBER(QSGSoftRenderableNode_m_hasClipRegion_tag, QSGSoftwareRenderableNode, m_hasClipRegion, bool);
W_DECLARE_PRIVATE_MEMBER(QSGSoftRenderableNode_m_opacity_tag, QSGSoftwareRenderableNode, m_opacity, float);

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

inline static WImageRenderTarget *getImageFrom(const QQuickRenderTarget &rt)
{
    auto d = QQuickRenderTargetPrivate::get(&rt);
    Q_ASSERT(d->type == QQuickRenderTargetPrivate::Type::PaintDevice);
    return static_cast<WImageRenderTarget*>(d->u.paintDevice);
}

static const wlr_drm_format *pickFormat(qw_renderer *renderer, uint32_t format)
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

static void applyTransform(QSGSoftwareRenderer *renderer, const QTransform &t)
{
    if (t.isIdentity())
        return;

    auto nodeIter = W_PRIVATE_MEMBER(*renderer, QSGAbsSoftRenderer_m_nodes_tag{}).begin();
    while (nodeIter != W_PRIVATE_MEMBER(*renderer, QSGAbsSoftRenderer_m_nodes_tag{}).end()) {
        auto node = *nodeIter;
        node->setTransform(node->transform() * t);

        if (W_PRIVATE_MEMBER(*node, QSGSoftRenderableNode_m_hasClipRegion_tag{}))
            node->setClipRegion(t.map(node->clipRegion()), true);

        ++nodeIter;
    }
}

WBufferRenderer::WBufferRenderer(QQuickItem *parent)
    : QQuickItem(parent)
    , m_cacheBuffer(true)
    , m_hideSource(false)
{
    // ensure graphical resources are released before scene graph is invalidated
    // since WBufferRenderer's ItemHasContent bit is unset
    // the invalidateSceneGraph slot will not be called through QQuickWindowPrivate::cleanupNodesOnShutdown
    QMetaObject::Connection windowConn;
    if (window())
        windowConn = connect(window(), &QQuickWindow::sceneGraphInvalidated, this, &WBufferRenderer::invalidateSceneGraph);
    connect(this, &QQuickItem::windowChanged, this, [this, windowConn](auto *window) mutable {
        disconnect(windowConn);
        if (window)
            windowConn = connect(window, &QQuickWindow::sceneGraphInvalidated, this, &WBufferRenderer::invalidateSceneGraph);
    });
}

WBufferRenderer::~WBufferRenderer()
{
    cleanTextureProvider();
    resetSources();
    cleanupRetiredResources(true);

    delete m_renderHelper;
    delete m_swapchain;
}

void WBufferRenderer::retireSwapchain(qw_swapchain *swapchain, bool defer)
{
    if (!swapchain)
        return;

    if (!defer) {
        delete swapchain;
        return;
    }

    m_retiredSwapchains.append(swapchain);
}

void WBufferRenderer::cleanupRetiredResources(bool force)
{
    if (m_renderHelper)
        m_renderHelper->cleanupRetiredRenderResources(force);

    qDeleteAll(m_retiredSwapchains);
    m_retiredSwapchains.clear();
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

int WBufferRenderer::sourceCount() const
{
    return m_sourceList.size();
}

QList<QQuickItem*> WBufferRenderer::sourceList() const
{
    QList<QQuickItem*> list;
    list.reserve(m_sourceList.size());

    for (const Data &i : std::as_const(m_sourceList))
        list.append(i.source);

    return list;
}

void WBufferRenderer::setSourceList(QList<QQuickItem*> sources, bool hideSource)
{
    bool changed = sources.size() != m_sourceList.size() || m_hideSource != hideSource;
    if (!changed) {
        for (int i = 0; i < sources.size(); ++i) {
            if (sources.at(i) != m_sourceList.at(i).source) {
                changed = true;
                break;
            }
        }
    }

    if (!changed)
        return;

    resetSources();
    m_hideSource = hideSource;

    for (auto s : std::as_const(sources)) {
        m_sourceList.append({s, nullptr});

        if (isRootItem(s))
            continue;

        connect(s, &QQuickItem::destroyed, this, [this] {
            const int index = indexOfSource(static_cast<QQuickItem*>(sender()));
            Q_ASSERT(index >= 0);
            // destroySource accesses m_sourceList[index], so must be called before removeAt
            destroySource(index);
            m_sourceList.removeAt(index);
        });

        auto d = QQuickItemPrivate::get(s);
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

void WBufferRenderer::lockCacheBuffer(QObject *owner)
{
    if (m_cacheBufferLocker.contains(owner))
        return;
    m_cacheBufferLocker.append(owner);
    connect(owner, &QObject::destroyed, this, [this] {
        unlockCacheBuffer(sender());
    });
    updateTextureProvider();
}

void WBufferRenderer::unlockCacheBuffer(QObject *owner)
{
    auto ok = m_cacheBufferLocker.removeOne(owner);
    Q_ASSERT(ok);
    ok = disconnect(owner, &QObject::destroyed, this, nullptr);
    Q_ASSERT(ok);
    updateTextureProvider();
}

QColor WBufferRenderer::clearColor() const
{
    return m_clearColor;
}

void WBufferRenderer::setClearColor(const QColor &clearColor)
{
    m_clearColor = clearColor;
}

QSGRenderer *WBufferRenderer::currentRenderer() const
{
    return state.renderer;
}

QSGBatchRenderer::Renderer *WBufferRenderer::currentBatchRenderer() const
{
    Q_ASSERT(state.renderer == state.batchRenderer);
    return state.batchRenderer;
}

qreal WBufferRenderer::currentDevicePixelRatio() const
{
    return state.devicePixelRatio;
}

const QMatrix4x4 &WBufferRenderer::currentWorldTransform() const
{
    return state.worldTransform;
}

qw_buffer *WBufferRenderer::currentBuffer() const
{
    return state.buffer.get();
}

qw_buffer *WBufferRenderer::lastBuffer() const
{
    return m_lastBuffer;
}

QRhiTexture *WBufferRenderer::currentRenderTarget() const
{
    auto renderTarget = state.activeSgRenderTarget.rt
                        ? state.activeSgRenderTarget.rt
                        : state.sgRenderTarget.rt;
    if (!renderTarget)
        return nullptr;
    auto textureRT = static_cast<QRhiTextureRenderTarget*>(renderTarget);
    auto colorAttachment = textureRT->description().colorAttachmentAt(0);
    if (!colorAttachment)
        return nullptr;
    return colorAttachment->texture();
}

const qw_damage_ring *WBufferRenderer::damageRing() const
{
    return &m_damageRing;
}

qw_damage_ring *WBufferRenderer::damageRing()
{
    return &m_damageRing;
}

bool WBufferRenderer::isTextureProvider() const
{
    if (isPrimaryOutputRendererForVulkan())
        return false;

    return true;
}

QSGTextureProvider *WBufferRenderer::textureProvider() const
{
    return wTextureProvider();
}

WSGTextureProvider *WBufferRenderer::wTextureProvider() const
{
    if (isPrimaryOutputRendererForVulkan())
        return nullptr;

    auto w = qobject_cast<WOutputRenderWindow*>(window());
    auto d = QQuickItemPrivate::get(this);
    if (!w || !d->sceneGraphRenderContext() || QThread::currentThread() != d->sceneGraphRenderContext()->thread()) {
        qCWarning(lcWlBufferRenderer, "WBufferRenderer::textureProvider: can only be queried on the rendering thread of an WOutputRenderWindow");
        return nullptr;
    }

    if (!m_textureProvider) {
        m_textureProvider.reset(new WSGTextureProvider(w));
        m_textureProvider->setBuffer(m_lastBuffer);
    }

    return m_textureProvider.get();
}

bool WBufferRenderer::isPrimaryOutputRendererForVulkan() const
{
    auto wd = window() ? QQuickWindowPrivate::get(window()) : nullptr;
    if (!wd || !wd->rhi || wd->rhi->backend() != QRhi::Vulkan)
        return false;

    auto viewport = qobject_cast<WOutputViewport *>(parentItem());
    if (!viewport)
        return false;

    auto vd = WOutputViewportPrivate::get(viewport);
    return vd && vd->attached && vd->bufferRenderer == this;
}

QTransform WBufferRenderer::inputMapToOutput(const QRectF &sourceRect, const QRectF &targetRect,
                                             const QSize &pixelSize, const qreal devicePixelRatio)
{
    Q_ASSERT(pixelSize.isValid());

    QTransform t;
    const auto outputSize = QSizeF(pixelSize) / devicePixelRatio;

    if (sourceRect.isValid())
        t.translate(-sourceRect.x(), -sourceRect.y());
    if (targetRect.isValid())
        t.translate(targetRect.x(), targetRect.y());

    if (sourceRect.isValid()) {
        t.scale(outputSize.width() / sourceRect.width(),
                outputSize.height() / sourceRect.height());
    }

    if (targetRect.isValid()) {
        t.scale(targetRect.width() / outputSize.width(),
                targetRect.height() / outputSize.height());
    }

    return t;
}

static QSGRenderTarget toRhiSgRenderTarget(QQuickWindowPrivate *wd, const QQuickRenderTarget &rt);

qw_buffer *WBufferRenderer::beginRender(const QSize &pixelSize, qreal devicePixelRatio,
                                        uint32_t format, RenderFlags flags)
{
    Q_ASSERT(!state.buffer);
    Q_ASSERT(m_output);

    if (pixelSize.isEmpty())
        return nullptr;

    Q_EMIT beforeRendering();

    auto wd = QQuickWindowPrivate::get(window());
    const bool isVulkanRhi = wd->rhi && wd->rhi->backend() == QRhi::Vulkan;

    // configure swapchain
    if (flags.testFlag(RenderFlag::DontConfigureSwapchain)) {
        auto renderFormat = pickFormat(m_output->renderer(), format);
        if (!renderFormat) {
            qCWarning(lcWlBufferRenderer, "wlr_renderer doesn't support format 0x%s", drmGetFormatName(format));
            return nullptr;
        }

        if (!m_swapchain || QSize(m_swapchain->handle()->width, m_swapchain->handle()->height) != pixelSize
            || m_swapchain->handle()->format.format != renderFormat->format) {
            retireSwapchain(m_swapchain, isVulkanRhi);
            m_swapchain = qw_swapchain::create(m_output->allocator()->handle(), pixelSize.width(), pixelSize.height(), renderFormat);
        }
    } else if (flags.testFlag(RenderFlag::UseCursorFormats)) {
        qw_swapchain *replacedSwapchain = nullptr;
        bool ok = m_output->configureCursorSwapchain(pixelSize, format, &m_swapchain,
                                                     isVulkanRhi ? &replacedSwapchain : nullptr);
        retireSwapchain(replacedSwapchain, isVulkanRhi);
        if (!ok)
            return nullptr;
    } else {
        qw_swapchain *replacedSwapchain = nullptr;
        bool ok = m_output->configurePrimarySwapchain(pixelSize, format, &m_swapchain,
                                                      !flags.testFlag(DontTestSwapchain),
                                                      isVulkanRhi ? &replacedSwapchain : nullptr);
        retireSwapchain(replacedSwapchain, isVulkanRhi);
        if (!ok)
            return nullptr;
    }

    // TODO: Support scanout buffer of wlr_surface(from WSurfaceItem)
    auto wbuffer = m_swapchain->acquire();
    if (!wbuffer)
        return nullptr;
    auto buffer = qw_buffer::from(wbuffer);

    if (!m_renderHelper)
        m_renderHelper = new WRenderHelper(m_output->renderer());
    m_renderHelper->setSize(pixelSize);

    Q_ASSERT(wd->renderControl);
    auto lastRT = m_renderHelper->lastRenderTarget();
    auto rt = m_renderHelper->acquireRenderTarget(wd->renderControl, buffer);
    if (rt.isNull()) {
        buffer->unlock();
        return nullptr;
    }

    // For software renderer, update the dirty parts relative to the last paint device.
    WPixmanRegion damage;
    m_damageRing.rotate_buffer(wbuffer, damage);
    state.dirty = WTools::fromPixmanRegion(damage);

    auto rtd = QQuickRenderTargetPrivate::get(&rt);
    QSGRenderTarget sgRT;

    if (rtd->type == QQuickRenderTargetPrivate::Type::PaintDevice) {
        sgRT.paintDevice = rtd->u.paintDevice;

        if (devicePixelRatio != 1.0) {
            state.dirty = QTransform::fromScale(1.0 / devicePixelRatio,
                                                1.0 / devicePixelRatio).map(state.dirty);
        }
    } else {
        state.dirty = QRegion();

        sgRT = toRhiSgRenderTarget(wd, rt);

#ifndef QT_NO_OPENGL
        if (wd->rhi->backend() == QRhi::OpenGLES2) {
            auto glRT = QRHI_RES(QGles2TextureRenderTarget, rtd->u.rhiRt);
            auto glContext = QOpenGLContext::currentContext();
            Q_ASSERT(glContext);
            QOpenGLContextPrivate::get(glContext)->defaultFboRedirect = glRT->framebuffer;
        }
#endif
    }

    state.flags = flags;
    state.context = wd->context;
    state.pixelSize = pixelSize;
    state.devicePixelRatio = devicePixelRatio;
    state.buffer.reset(buffer);
    state.renderTarget = rt;
    state.sgRenderTarget = sgRT;
    state.activeSgRenderTarget = sgRT;
    state.preserveRenderTarget = m_renderHelper->preserveRenderTarget(buffer);
    if (!state.preserveRenderTarget.isNull()) {
        auto preserveRtd = QQuickRenderTargetPrivate::get(&state.preserveRenderTarget);
        if (preserveRtd->type == QQuickRenderTargetPrivate::Type::RhiRenderTarget)
            state.preserveSgRenderTarget = toRhiSgRenderTarget(wd, state.preserveRenderTarget);
    }
    state.renderBufferReleasedForCache = false;

    return buffer;
}

inline static QRect scaleToRect(const QRectF &s, qreal scale) {
    return QRect((s.topLeft() * scale).toPoint(),
                 (s.size() * scale).toSize());
}

static QSGRenderTarget toRhiSgRenderTarget(QQuickWindowPrivate *wd, const QQuickRenderTarget &rt)
{
    auto rtd = QQuickRenderTargetPrivate::get(&rt);
    Q_ASSERT(rtd->type == QQuickRenderTargetPrivate::Type::RhiRenderTarget);

    QSGRenderTarget sgRT;
    sgRT.rt = rtd->u.rhiRt;
    sgRT.cb = wd->redirect.commandBuffer;
    Q_ASSERT(sgRT.cb);
    sgRT.rpDesc = rtd->u.rhiRt->renderPassDescriptor();
    return sgRT;
}

static QVector<WSGTextureProvider *> activeTextureProvidersForPass(QQuickItem *root)
{
    QVector<WSGTextureProvider *> providers;
    if (!root)
        return providers;

    QSet<WSGTextureProvider *> seen;
    const auto items = WOutputRenderWindow::paintOrderItemList(root, [] (QQuickItem *item) {
        return item
               && item->isVisible()
               && item->flags().testFlag(QQuickItem::ItemHasContents)
               && item->isTextureProvider();
    });

    providers.reserve(items.size());
    for (auto item : items) {
        if (!item)
            continue;

        auto provider = qobject_cast<WSGTextureProvider *>(item->textureProvider());
        if (!provider || seen.contains(provider))
            continue;

        seen.insert(provider);
        providers.append(provider);
    }

    return providers;
}

bool WBufferRenderer::render(int sourceIndex, const QMatrix4x4 &renderMatrix,
                             const QRectF &sourceRect, const QRectF &targetRect,
                             bool preserveColorContents)
{
    Q_ASSERT(state.buffer);

    const auto &source = m_sourceList.at(sourceIndex);
    QSGRenderer *renderer = ensureRenderer(sourceIndex, state.context);
    auto wd = QQuickWindowPrivate::get(window());

    const qreal devicePixelRatio = state.devicePixelRatio;
    state.renderer = renderer;
    state.batchRenderer = dynamic_cast<QSGBatchRenderer::Renderer*>(renderer);
    state.worldTransform = renderMatrix;
    const bool isVulkanRhi = wd->rhi && wd->rhi->backend() == QRhi::Vulkan;
    auto activeRenderTarget = state.renderTarget;
    auto activeSgRenderTarget = state.sgRenderTarget;
    if (isVulkanRhi && preserveColorContents && state.preserveSgRenderTarget.rt) {
        activeRenderTarget = state.preserveRenderTarget;
        activeSgRenderTarget = state.preserveSgRenderTarget;
        qCDebug(lcWlBufferRenderer) << "Using Vulkan preserve render target for render pass"
                                    << "renderer" << this
                                    << "sourceIndex" << sourceIndex
                                    << "buffer" << state.buffer.get()
                                    << "renderTarget" << activeSgRenderTarget.rt;
    }
    state.activeSgRenderTarget = activeSgRenderTarget;
    // The renderer should always receive the window's DPR (Device Pixel Ratio)
    // because, regardless of the DPR used for rendering, all resources within
    // a window are loaded based on the window's own DPR.

    // During rendering, certain specialized nodes (e.g., QSGCurveStrokeMaterialShader)
    // use QSGRenderer::devicePixelRatio for specific calculations related to
    // ShapePath::fillItem. When displaying this fillItem using Shape, it might
    // be desirable for the QSGTexture provided by fillItem to scale completely
    // to match the size of the Shape, regardless of its original size.

    // If a PathRectangle is used, its width is set to
    // textureSize.width / QQuickWindow::effectiveDevicePixelRatio. Here, the
    // devicePixelRatio value is utilized because the width and height passed
    // to PathRectangle need to be converted from pixel values to pixel-independent sizes.

    // Returning to the issue mentioned earlier, QSGCurveStrokeMaterialShader
    // uses QSGRenderer::devicePixelRatio for additional calculations that influence
    // how Shape fills the QSGTexture provided by fillItem. Therefore, we need to ensure
    // that QSGRenderer::devicePixelRatio and QQuickWindow::effectiveDevicePixelRatio
    // are always consistent. Otherwise, some Items might render incorrectly.
    renderer->setDevicePixelRatio(window()->effectiveDevicePixelRatio());
    renderer->setDeviceRect(QRect(QPoint(0, 0), state.pixelSize));
    renderer->setRenderTarget(activeSgRenderTarget);
    const auto viewportRect = scaleToRect(targetRect, devicePixelRatio);

    auto softwareRenderer = dynamic_cast<QSGSoftwareRenderer*>(renderer);
    { // before render
        if (softwareRenderer) {
            // Avoid do clear before paint, for the software renderer this
            // work is expensive.
            if (m_clearColor.alpha() == 0)
                preserveColorContents = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
            softwareRenderer->setClearColorEnabled(!preserveColorContents);
#else
            auto bn = softwareRenderer->renderableNode(W_PRIVATE_MEMBER(*softwareRenderer, QSGAbsSoftRenderer_m_background_tag{}));
            if (bn) {
                W_PRIVATE_MEMBER(*bn, QSGSoftRenderableNode_m_opacity_tag{}) = preserveColorContents ? 0 : 1;
            }
#endif
            if (!state.dirty.isEmpty()) {
                W_PRIVATE_MEMBER(*softwareRenderer, QSGAbsSoftRenderer_m_dirtyRegion_tag{}) += state.dirty;
                state.dirty = QRegion();
            }

            // because software renderer don't supports viewportRect,
            // so use transform to simulation.
            const auto mapTransform = inputMapToOutput(sourceRect, targetRect,
                                                       state.pixelSize, state.devicePixelRatio);
            if (!mapTransform.isIdentity())
                state.worldTransform = mapTransform * state.worldTransform;
            state.worldTransform.optimize();
            auto image = getImageFrom(state.renderTarget);
            image->setDevicePixelRatio(devicePixelRatio);

            // TODO: Should set to QSGSoftwareRenderer, but it's not support specify matrix.
            // If transform is changed, it will full repaint.
            if (isRootItem(source.source)) {
                auto rootTransform = QQuickItemPrivate::get(wd->contentItem)->itemNode();
                if (rootTransform->matrix() != state.worldTransform)
                    rootTransform->setMatrix(state.worldTransform);
            } else {
                auto t = state.worldTransform.toTransform();
                if (t.type() > QTransform::TxTranslate) {
                    (image->operator QImage &()).fill(renderer->clearColor());
                    softwareRenderer->markDirty();
                }

                applyTransform(softwareRenderer, t);
            }
        } else {
            state.worldTransform.optimize();

            bool flipY = wd->rhi ? !wd->rhi->isYUpInNDC() : false;
            if (activeRenderTarget.mirrorVertically())
                flipY = !flipY;

            if (viewportRect.isValid()) {
                QRect vr = viewportRect;
                if (flipY)
                    vr.moveTop(-vr.y() + state.pixelSize.height() - vr.height());
                renderer->setViewportRect(vr);
            } else {
                renderer->setViewportRect(QRect(QPoint(0, 0), state.pixelSize));
            }

            QRectF rect = sourceRect;
            if (!rect.isValid())
                rect = QRectF(QPointF(0, 0), QSizeF(state.pixelSize) / devicePixelRatio);

            const float left = rect.x();
            const float right = rect.x() + rect.width();
            float bottom = rect.y() + rect.height();
            float top = rect.y();

            if (flipY)
                std::swap(top, bottom);

            QMatrix4x4 matrix;
            matrix.ortho(left, right, bottom, top, 1, -1);

            QMatrix4x4 projectionMatrix, projectionMatrixWithNativeNDC;
            projectionMatrix = matrix * state.worldTransform;

            if (wd->rhi && !wd->rhi->isYUpInNDC()) {
                std::swap(top, bottom);

                matrix.setToIdentity();
                matrix.ortho(left, right, bottom, top, 1, -1);
            }
            projectionMatrixWithNativeNDC = matrix * state.worldTransform;

            renderer->setProjectionMatrix(projectionMatrix);
            renderer->setProjectionMatrixWithNativeNDC(projectionMatrixWithNativeNDC);

            auto textureRT = static_cast<QRhiTextureRenderTarget*>(activeSgRenderTarget.rt);
            if (!isVulkanRhi) {
                if (preserveColorContents) {
                    textureRT->setFlags(textureRT->flags() | QRhiTextureRenderTarget::PreserveColorContents);
                } else {
                    textureRT->setFlags(textureRT->flags() & ~QRhiTextureRenderTarget::PreserveColorContents);
                }
            }
        }
    }

    QVector<qw_texture *> preparedTextures;
    auto outputWindow = renderWindow();
    const auto activeTextureProviders = isVulkanRhi
        ? activeTextureProvidersForPass(isRootItem(source.source)
                                            ? wd->contentItem
                                            : source.source)
        : QVector<WSGTextureProvider *> {};
    constexpr const char *samplingPurpose = "qt-render-pass-texture";
    WVulkanTrace::beginPass(outputWindow, this, state.buffer.get(), samplingPurpose,
                            sourceIndex, activeTextureProviders.size());
    if (outputWindow
        && !outputWindow->prepareTextureSamplingForRenderPass(state.buffer.get(),
                                                              activeTextureProviders,
                                                              samplingPurpose,
                                                              sourceIndex,
                                                              &preparedTextures)) {
        qCWarning(lcWlBufferRenderer) << "Skipping render pass because Vulkan texture sampling prepare failed"
                                      << "renderer" << this
                                      << "sourceIndex" << sourceIndex
                                      << "currentBuffer" << state.buffer.get()
                                      << "wlrBuffer" << (state.buffer ? state.buffer->handle() : nullptr);
        WVulkanTrace::endPass(outputWindow, false);
        return false;
    }

    state.context->renderNextFrame(renderer);

    { // after render
        if (!softwareRenderer) {
            // TODO: get damage area from QRhi renderer
            m_damageRing.add_whole();
            // ###: maybe Qt bug? Before executing QRhi::endOffscreenFrame, we may
            // use the same QSGRenderer for multiple drawings. This can lead to
            // rendering the same content for different QSGRhiRenderTarget instances
            // when using the RhiGles backend. Additionally, considering that the
            // result of the current drawing may be needed when drawing the next
            // sourceIndex, we should let the RHI (Rendering Hardware Interface)
            // complete the results of this drawing here to ensure the current
            // drawing result is available for use.
            if (!isVulkanRhi) {
                wd->rhi->finish();
            }
        } else {
            state.dirty = softwareRenderer->flushRegion();

            auto currentImage = getImageFrom(state.renderTarget);
            Q_ASSERT(currentImage && currentImage == softwareRenderer->renderTarget().paintDevice);
            currentImage->setDevicePixelRatio(1.0);
            const auto scaleTF = QTransform::fromScale(devicePixelRatio, devicePixelRatio);
            const auto scaledFlushRegion = scaleTF.map(softwareRenderer->flushRegion());
            WPixmanRegion scaledFlushDamage;
            bool ok = WTools::toPixmanRegion(scaledFlushRegion, scaledFlushDamage);
            Q_ASSERT(ok);

            {
                if (viewportRect.isValid()) {
                    QRect imageRect = (currentImage->operator const QImage &()).rect();
                    QRegion invalidRegion(imageRect);
                    invalidRegion -= viewportRect;
                    if (!scaledFlushRegion.isEmpty())
                        invalidRegion &= scaledFlushRegion;

                    if (!invalidRegion.isEmpty()) {
                        QPainter pa(currentImage);
                        for (const auto r : std::as_const(invalidRegion))
                            pa.fillRect(r, softwareRenderer->clearColor());
                    }
                }
            }

            if (!isRootItem(source.source))
                applyTransform(softwareRenderer, state.worldTransform.inverted().toTransform());
            m_damageRing.add(scaledFlushDamage);
        }
    }

    if (auto dr = qobject_cast<QSGDefaultRenderContext*>(state.context)) {
        QRhiResourceUpdateBatch *resourceUpdates = wd->rhi->nextResourceUpdateBatch();
        dr->currentFrameCommandBuffer()->resourceUpdate(resourceUpdates);
    }

    if (outputWindow
        && !outputWindow->finishTextureSamplingForRenderPass(preparedTextures,
                                                             samplingPurpose,
                                                             sourceIndex)) {
        qCWarning(lcWlBufferRenderer) << "Skipping render pass because Vulkan texture sampling finish failed"
                                      << "renderer" << this
                                      << "sourceIndex" << sourceIndex
                                      << "currentBuffer" << state.buffer.get()
                                      << "wlrBuffer" << (state.buffer ? state.buffer->handle() : nullptr)
                                      << "preparedTextureCount" << preparedTextures.size();
        WVulkanTrace::endPass(outputWindow, false);
        return false;
    }

    WVulkanTrace::endPass(outputWindow, true);

    if (shouldCacheBuffer() && !isVulkanRhi) {
        wTextureProvider()->setBuffer(state.buffer.get());
    }

    return true;
}

void WBufferRenderer::endRender()
{
    Q_ASSERT(state.buffer.get());
    auto wd = QQuickWindowPrivate::get(window());
    const bool isVulkanRhi = wd->rhi && wd->rhi->backend() == QRhi::Vulkan;
    const bool canExposeCompletedCache = state.renderBufferReleasedForCache;
    {
        std::unique_ptr<qw_buffer, qw_buffer::unlocker> buffer;
        buffer.swap(state.buffer);
        state.renderer = nullptr;
        state.batchRenderer = nullptr;
        state.activeSgRenderTarget = {};
        state.preserveRenderTarget = {};
        state.preserveSgRenderTarget = {};

        m_lastBuffer = buffer.get();
        if (shouldCacheBuffer() && isVulkanRhi) {
            if (isPrimaryOutputRendererForVulkan()) {
                if (m_textureProvider)
                    m_textureProvider->setBuffer(nullptr);
            } else if (canExposeCompletedCache) {
                if (auto provider = wTextureProvider())
                    provider->setBuffer(buffer.get());
            }
        }
    }
    state.renderBufferReleasedForCache = false;

#ifndef QT_NO_OPENGL
    if (state.flags.testFlag(RedirectOpenGLContextDefaultFrameBufferObject)
        && wd->rhi && wd->rhi->backend() == QRhi::OpenGLES2) {
        auto glContext = QOpenGLContext::currentContext();
        Q_ASSERT(glContext);
        QOpenGLContextPrivate::get(glContext)->defaultFboRedirect = GL_NONE;
    }
#endif

    Q_EMIT afterRendering();
}

void WBufferRenderer::componentComplete()
{
    QQuickItem::componentComplete();
}

void WBufferRenderer::updateTextureProvider()
{
    if (!m_textureProvider)
        return;

    if (isPrimaryOutputRendererForVulkan()) {
        m_textureProvider->setBuffer(nullptr);
        return;
    }

    if (shouldCacheBuffer()) {
        const bool hasCachedBuffer = m_textureProvider->qwBuffer();
        // Ensure only update the buffer when the "shouldCacheBuffer" state is changed.
        // If the state is not changed, the buffer is update in the WBufferRenderer::render.
        if (!hasCachedBuffer && m_lastBuffer)
            m_textureProvider->setBuffer(m_lastBuffer);
    } else {
        m_textureProvider->setBuffer(nullptr);
    }
}

QSGNode *WBufferRenderer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto texture = m_textureProvider ? m_textureProvider->texture() : nullptr;
    if (!texture) {
        delete oldNode;
        return nullptr;
    }

    auto node = static_cast<QSGImageNode*>(oldNode);
    if (Q_UNLIKELY(!node)) {
        node = window()->createImageNode();
        node->setOwnsTexture(false);
        node->setTexture(texture);
    } else {
        node->setTexture(texture);
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
    cleanupRetiredResources(true);
    resetSources();
}

void WBufferRenderer::releaseResources()
{
    cleanTextureProvider();
    cleanupRetiredResources(true);
    resetSources();
}

void WBufferRenderer::cleanTextureProvider()
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

        m_textureProvider->invalidate();
        // Delay clean the textures on the next render after.
        // Only schedule render job if window is still valid
        if (window()) {
            window()->scheduleRenderJob(new TextureProviderCleanupJob(m_textureProvider.release()),
                                        QQuickWindow::AfterRenderingStage);
        } else {
            // Window is being destroyed, clean up immediately
            m_textureProvider.reset();
        }
    }
}

void WBufferRenderer::resetSources()
{
    for (int i = 0; i < m_sourceList.size(); ++i) {
        destroySource(i);
    }
    m_sourceList.clear();
}

void WBufferRenderer::destroySource(int index)
{
    auto &s = m_sourceList[index];
    if (isRootItem(s.source))
        return;

    // Renderer of source is delay initialized in ensureRenderer. It might be null here.
    if (s.renderer) {
        delete s.renderer;
        s.renderer = nullptr;
    }
    s.source->disconnect(this);

    auto d = QQuickItemPrivate::get(s.source);
    if (d->inDestructor)
        return;

    d->derefFromEffectItem(m_hideSource);
}

int WBufferRenderer::indexOfSource(QQuickItem *s)
{
    for (int i = 0; i < m_sourceList.size(); ++i) {
        if (m_sourceList.at(i).source == s) {
            return i;
        }
    }

    return -1;
}

QSGRenderer *WBufferRenderer::ensureRenderer(int sourceIndex, QSGRenderContext *rc)
{
    Data &d = m_sourceList[sourceIndex];
    if (isRootItem(d.source))
        return QQuickWindowPrivate::get(window())->renderer;

    if (Q_LIKELY(d.renderer))
        return d.renderer;

    auto rootNode = WQmlHelper::getRootNode(d.source);
    Q_ASSERT(rootNode);

    auto dr = qobject_cast<QSGDefaultRenderContext*>(rc);
    const bool useDepth = dr ? dr->useDepthBufferFor2D() : false;
    const auto renderMode = useDepth ? QSGRendererInterface::RenderMode2D
                                     : QSGRendererInterface::RenderMode2DNoDepthBuffer;
    d.renderer = rc->createRenderer(renderMode);
    d.renderer->setRootNode(rootNode);
    QObject::connect(d.renderer, &QSGRenderer::sceneGraphChanged,
                     this, &WBufferRenderer::sceneGraphChanged);

    d.renderer->setClearColor(m_clearColor);

    return d.renderer;
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wbufferrenderer_p.cpp"
