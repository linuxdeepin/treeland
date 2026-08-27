// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wscopedvalue.h>
#include <wpointer.h>
#include "wbufferrenderer_p.h"
#include "wayliblogging.h"
#include "wrenderhelper.h"
#include "wqmlhelper_p.h"
#include "wtools.h"
#include "wsgtextureprovider.h"
#include "private/wprivateaccessor_p.h"
#include "wsgbatchrenderer_p.h"
#include "wsgdamagetracker_p.h"
#include "wsgdamagenode_p.h"
#include "woutput.h"
#include "woutputrenderwindow.h"

#include <wlr_all.h>

#include <QSGImageNode>
#include <QSGSimpleRectNode>
#include <QPainter>

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

#include <pixman.h>
#include <drm_fourcc.h>
#include <xf86drm.h>

using QSGAbsSoftRenderer_NodesMap = QHash<QSGNode*, QSGSoftwareRenderableNode*>;
W_DECLARE_PRIVATE_MEMBER(QSGAbsSoftRenderer_m_nodes_tag, QSGAbstractSoftwareRenderer, m_nodes, QSGAbsSoftRenderer_NodesMap);
W_DECLARE_PRIVATE_MEMBER(QSGAbsSoftRenderer_m_background_tag, QSGAbstractSoftwareRenderer, m_background, QSGSimpleRectNode*);
W_DECLARE_PRIVATE_MEMBER(QSGAbsSoftRenderer_m_dirtyRegion_tag, QSGAbstractSoftwareRenderer, m_dirtyRegion, QRegion);
W_DECLARE_PRIVATE_MEMBER(QSGSoftRenderableNode_m_hasClipRegion_tag, QSGSoftwareRenderableNode, m_hasClipRegion, bool);
W_DECLARE_PRIVATE_MEMBER(QSGSoftRenderableNode_m_opacity_tag, QSGSoftwareRenderableNode, m_opacity, float);

WAYLIB_SERVER_BEGIN_NAMESPACE

inline static WImageRenderTarget *getImageFrom(const QQuickRenderTarget &rt)
{
    auto d = QQuickRenderTargetPrivate::get(&rt);
    Q_ASSERT(d->type == QQuickRenderTargetPrivate::Type::PaintDevice);
    return static_cast<WImageRenderTarget*>(d->u.paintDevice);
}

static const wlr_drm_format *pickFormat(wlr_renderer *renderer, uint32_t format)
{
    auto r = renderer;
    if (!r->impl->get_render_formats) {
        return nullptr;
    }
    const wlr_drm_format_set *format_set = r->impl->get_render_formats(r);
    if (!format_set)
        return nullptr;

    return wlr_drm_format_set_get(format_set, format);
}

// 1. Logical layer: Item scene coordinates -> output logical coordinates (dp)
static QMatrix4x4 sceneToOutputTransform(const QMatrix4x4 &worldTransform,
                                         const QRectF &sourceRect,
                                         const QRectF &targetRect,
                                         const QSize &pixelSize,
                                         qreal devicePixelRatio)
{
    QMatrix4x4 m(WSGViewport::inputMapToOutput(sourceRect, targetRect, pixelSize, devicePixelRatio));
    return m * worldTransform;
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
    , m_pendingFullDamage(false)
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

    delete m_renderHelper;
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
    // A window renderer is shared and cannot own a downstream dependency tree.
    if (std::find(sources.cbegin() + qMin(1, sources.size()), sources.cend(), nullptr)
        != sources.cend()) {
        qCWarning(lcWlBufferRenderer) << "The window source is only valid at source index 0";
        return;
    }
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
    markFullDamage();
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
            markFullDamage();
            Q_EMIT sceneGraphChanged();
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

WSGBatchRenderer::Renderer *WBufferRenderer::currentBatchRenderer() const
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

wlr_buffer *WBufferRenderer::currentBuffer() const
{
    return state.buffer.get();
}

wlr_buffer *WBufferRenderer::lastBuffer() const
{
    return m_lastBuffer;
}

void WBufferRenderer::markFullDamage()
{
    m_pendingFullDamage = true;
}

QRhiTexture *WBufferRenderer::currentRenderTarget() const
{
    if (!state.sgRenderTarget.rt)
        return nullptr;
    auto textureRT = static_cast<QRhiTextureRenderTarget*>(state.sgRenderTarget.rt);
    auto colorAttachment = textureRT->description().colorAttachmentAt(0);
    if (!colorAttachment)
        return nullptr;
    return colorAttachment->texture();
}

bool WBufferRenderer::isColorPreserved() const
{
    return state.renderTarget.colorPreserved();
}

const wlr_damage_ring *WBufferRenderer::damageRing() const
{
    return m_damageRing.get();
}

wlr_damage_ring *WBufferRenderer::damageRing()
{
    return m_damageRing.get();
}

bool WBufferRenderer::isTextureProvider() const
{
    return true;
}

QSGTextureProvider *WBufferRenderer::textureProvider() const
{
    return wTextureProvider();
}

WSGTextureProvider *WBufferRenderer::wTextureProvider() const
{
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

wlr_buffer *WBufferRenderer::beginRender(const QSize &pixelSize,
                                        uint32_t format, RenderFlags flags,
                                        WGlobal::ColorContentsMode mode)
{
    Q_ASSERT(!state.buffer);
    Q_ASSERT(m_output);

    if (pixelSize.isEmpty())
        return nullptr;

    if (!m_swapchain || QSize(m_swapchain->width, m_swapchain->height) != pixelSize)
        markFullDamage();
    // New render cycle: reset the per-cycle flush and render accumulators.
    m_lastFlushRegion.clear();
    m_lastRenderRegion.clear();
    state.flushRegion.clear();
    state.renderRegion.clear();
    Q_EMIT beforeRendering();

    // configure swapchain
    if (flags.testFlag(RenderFlag::DontConfigureSwapchain)) {
        auto renderFormat = pickFormat(m_output->renderer(), format);
        if (!renderFormat) {
            qCWarning(lcWlBufferRenderer, "wlr_renderer doesn't support format 0x%s", drmGetFormatName(format));
            return nullptr;
        }

        if (!m_swapchain || QSize(m_swapchain->width, m_swapchain->height) != pixelSize
            || m_swapchain->format.format != renderFormat->format) {
            m_swapchain.reset(wlr_swapchain_create(m_output->allocator(), pixelSize.width(), pixelSize.height(), renderFormat));
            if (!m_swapchain)
                return nullptr;
        }
    } else if (flags.testFlag(RenderFlag::UseCursorFormats)) {
        // The configure helpers take a wlr_swapchain**; hand ownership out
        // temporarily and take it back (possibly a new swapchain) afterwards.
        wlr_swapchain *sc = m_swapchain.release();
        bool ok = m_output->configureCursorSwapchain(pixelSize, format, &sc);
        if (!ok) {
            m_swapchain.reset(sc);
            return nullptr;
        }
        m_swapchain.reset(sc);
    } else {
        wlr_swapchain *sc = m_swapchain.release();
        bool ok = m_output->configurePrimarySwapchain(pixelSize, format, &sc,
                                                      !flags.testFlag(DontTestSwapchain));
        if (!ok) {
            m_swapchain.reset(sc);
            return nullptr;
        }
        m_swapchain.reset(sc);
    }

    // TODO: Support scanout buffer of wlr_surface(from WSurfaceItem)
    auto buffer = wlr_swapchain_acquire(m_swapchain.get());
    if (!buffer)
        return nullptr;

    if (!m_renderHelper)
        m_renderHelper = new WRenderHelper(m_output->renderer());
    m_renderHelper->setSize(pixelSize);

    auto wd = QQuickWindowPrivate::get(window());
    Q_ASSERT(wd->renderControl);
    auto rt = m_renderHelper->acquireRenderTarget(wd->renderControl, buffer, mode);
    if (rt.isNull()) {
        wlr_buffer_unlock(buffer);
        return nullptr;
    }

    state.dirty.clear();
    state.needsRotateBuffer = true;

    auto rtValue = rt.rt();
    auto rtd = QQuickRenderTargetPrivate::get(&rtValue);
    QSGRenderTarget sgRT;

    if (rtd->type == QQuickRenderTargetPrivate::Type::PaintDevice) {
        sgRT.paintDevice = rtd->u.paintDevice;
    } else {
        Q_ASSERT(rtd->type == QQuickRenderTargetPrivate::Type::RhiRenderTarget);
        sgRT.rt = rtd->u.rhiRt;
        sgRT.cb = wd->redirect.commandBuffer;
        Q_ASSERT(sgRT.cb);
        sgRT.rpDesc = rtd->u.rhiRt->renderPassDescriptor();

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
    state.colorContentsMode = mode;
    state.context = wd->context;
    state.pixelSize = pixelSize;
    state.buffer.reset(buffer);
    state.renderTarget = rt;
    state.sgRenderTarget = sgRT;

    return buffer;
}

inline static QRect scaleToRect(const QRectF &s, qreal scale) {
    return QRect((s.topLeft() * scale).toPoint(),
                 (s.size() * scale).toSize());
}

void WBufferRenderer::render(int sourceIndex, WSGViewport &viewport)
{
    Q_ASSERT(state.buffer);

    const QRect bufferRect(QPoint(), state.pixelSize);
    const auto &source = m_sourceList.at(sourceIndex);
    QSGRenderer *renderer = ensureRenderer(sourceIndex, state.context);
    const bool tracksDamageEarly = dynamic_cast<WSGBatchRenderer::Renderer*>(renderer)
        && static_cast<WSGBatchRenderer::Renderer*>(renderer)->damageMode()
            != WSGBatchRenderer::Renderer::DamageMode::Off;
    const bool selfManagedDamage = tracksDamageEarly && source.renderer;
    // Independently owned sources commit below. Shared viewports were already
    // committed by the render window.
    if (m_pendingFullDamage) {
        viewport.markFull();
        m_pendingFullDamage = false;
    }

    const QMatrix4x4 &renderMatrix = viewport.renderMatrix();
    const QRectF sourceRect = viewport.sourceRect();
    const QRectF targetRect = viewport.targetRect();
    auto wd = QQuickWindowPrivate::get(window());

    const qreal devicePixelRatio = viewport.devicePixelRatio();
    state.devicePixelRatio = devicePixelRatio;
    state.renderer = renderer;
    state.batchRenderer = dynamic_cast<WSGBatchRenderer::Renderer*>(renderer);
    state.worldTransform = renderMatrix;
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
    renderer->setDeviceRect(bufferRect);
    renderer->setRenderTarget(state.sgRenderTarget);
    const auto viewportRect = scaleToRect(targetRect, devicePixelRatio);

    auto softwareRenderer = dynamic_cast<QSGSoftwareRenderer*>(renderer);
    const bool isVulkanRhi = wd->rhi && wd->rhi->backend() == QRhi::Vulkan;
    { // before render
        if (softwareRenderer) {
            // Avoid do clear before paint, for the software renderer this
            // work is expensive.
            const bool clearColor = !state.renderTarget.colorPreserved();
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
            softwareRenderer->setClearColorEnabled(clearColor);
#else
            auto bn = softwareRenderer->renderableNode(W_PRIVATE_MEMBER(*softwareRenderer, QSGAbsSoftRenderer_m_background_tag{}));
            if (bn) {
                W_PRIVATE_MEMBER(*bn, QSGSoftRenderableNode_m_opacity_tag{}) = clearColor ? 1 : 0;
            }
#endif
            ensureRotateBuffer();
            if (!state.dirty.isEmpty()) {
                W_PRIVATE_MEMBER(*softwareRenderer, QSGAbsSoftRenderer_m_dirtyRegion_tag{}) += state.dirty.toQRegion();
                state.dirty.clear();
            }

            // Because QSGSoftwareRenderer does not support hardware viewportRect, simulate
            // it by pre-baking the logical scene-to-output mapping into worldTransform (dp).
            state.worldTransform = sceneToOutputTransform(state.worldTransform, sourceRect, targetRect,
                                                          state.pixelSize, state.devicePixelRatio);
            state.worldTransform.optimize();
            auto image = getImageFrom(state.renderTarget.rt());
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
                    if (clearColor)
                        (image->operator QImage &()).fill(renderer->clearColor());
                    softwareRenderer->markDirty();
                }

                applyTransform(softwareRenderer, t);
            }
        } else {
            state.worldTransform.optimize();

            bool flipY = wd->rhi ? !wd->rhi->isYUpInNDC() : false;
            if (state.renderTarget.rt().mirrorVertically())
                flipY = !flipY;

            if (viewportRect.isValid()) {
                QRect vr = viewportRect;
                if (flipY)
                    vr.moveTop(-vr.y() + state.pixelSize.height() - vr.height());
                renderer->setViewportRect(vr);
            } else {
                renderer->setViewportRect(bufferRect);
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

        }
    }

#ifdef ENABLE_VULKAN_RENDER
    if (m_renderHelper)
        m_renderHelper->prepareVulkanRenderTarget(state.sgRenderTarget.cb, state.renderTarget);
#endif
    const bool tracksDamage = state.batchRenderer
        && state.batchRenderer->damageMode() != WSGBatchRenderer::Renderer::DamageMode::Off;


    QMatrix4x4 toBuffer;
    if (tracksDamage) {
        toBuffer = viewport.sceneToBufferTransform(state.pixelSize);
        bool invertible = false;
        const QMatrix4x4 fromBuffer = toBuffer.inverted(&invertible);
        const QRect sceneOutputRect = invertible ? mapOuter(fromBuffer, bufferRect) : QRect();

        state.batchRenderer->setDamageScissorTarget(state.sgRenderTarget.rt);

        if (invertible && !viewport.isFull()) {
            if (sourceIndex > 0 && selfManagedDamage) {
                auto *background = state.batchRenderer->backgroundDamageNode();
                background->setBoundingRect(sceneOutputRect);
                if (!state.flushRegion.isEmpty()) {
                    WPixmanRegion content = state.flushRegion.mappedOuter(fromBuffer);
                    content &= sceneOutputRect;
                    content.translate(-sceneOutputRect.x(), -sceneOutputRect.y());
                    background->markContentDirty(content.native());
                }
            }
            if (selfManagedDamage) {
                if (auto *tracker = state.batchRenderer->damageTracker())
                    tracker->commit(viewport);
            }
        }

        // Shared scenes were committed for all outputs by the render window.
        // Independently owned sources commit above. isFull after that is a
        // whole-buffer redraw, not a WPixmanRegion.
        const WDamageRegion &committed = viewport.damageRegion();
        if (!invertible || committed.isFull) {
            ensureRotateBuffer();
            state.batchRenderer->setDamage(WDamageRegion(true), WDamageRegion(true), sceneOutputRect);
        } else {
            WPixmanRegion scene = sceneOutputRect.isEmpty()
                ? committed.region
                : committed.region & sceneOutputRect;

            const bool hasDamage = !scene.isEmpty();
            if (hasDamage)
                ensureRotateBuffer();

            WPixmanRegion bufferAgeInScene;
            if (!softwareRenderer && state.renderTarget.colorPreserved() && !state.dirty.isEmpty())
                bufferAgeInScene = state.dirty.mappedOuter(fromBuffer);
            WPixmanRegion gpu = scene + bufferAgeInScene;
            if (sourceIndex > 0)
                gpu += state.renderRegion.mappedOuter(fromBuffer);
            if (!sceneOutputRect.isEmpty())
                gpu &= sceneOutputRect;

            const bool coversOutput =
                !sceneOutputRect.isEmpty() && (WPixmanRegion(sceneOutputRect) - gpu).isEmpty();
            const bool sceneCoversOutput =
                !sceneOutputRect.isEmpty() && (WPixmanRegion(sceneOutputRect) - scene).isEmpty();

            const WDamageRegion renderDamage =
                coversOutput ? WDamageRegion(true) : WDamageRegion(gpu, false);
            const WDamageRegion flushDamage =
                sceneCoversOutput ? WDamageRegion(true) : WDamageRegion(scene, false);

            state.batchRenderer->setDamage(flushDamage, renderDamage, sceneOutputRect);
        }
    } else {
        ensureRotateBuffer();
    }
    state.context->renderNextFrame(renderer);
    if (tracksDamage) {
        state.batchRenderer->setDamageScissorTarget(nullptr);
        if (selfManagedDamage) {
            if (auto *tracker = state.batchRenderer->damageTracker())
                tracker->finishFrame();
        }
    }
#ifdef ENABLE_VULKAN_RENDER
    if (m_renderHelper)
        m_renderHelper->finishVulkanRenderTarget(state.sgRenderTarget.cb, state.renderTarget);
#endif

    { // after render
        if (!softwareRenderer) {
            if (tracksDamage) {
                m_lastRenderRegion += state.batchRenderer->lastRenderRegion();
                m_lastFlushRegion += state.batchRenderer->lastFlushRegion();

                const auto bufferDamageFor = [&](const WDamageRegion &damage) {
                    WPixmanRegion bufferDamage = damage.isFull
                        ? WPixmanRegion(bufferRect) : damage.region.mappedOuter(toBuffer);
                    if (viewportRect.isValid())
                        bufferDamage &= viewportRect;
                    bufferDamage &= bufferRect;
                    return bufferDamage;
                };
                const WPixmanRegion flushRegion = bufferDamageFor(state.batchRenderer->lastFlushRegion());
                const WPixmanRegion renderRegion = bufferDamageFor(state.batchRenderer->lastRenderRegion());
                state.flushRegion += flushRegion;
                state.renderRegion += renderRegion;
                if (!flushRegion.isEmpty())
                    wlr_damage_ring_add(m_damageRing.get(), flushRegion);
                qCDebug(lcWlBufferRenderer) << "RHI buffer render damage" << renderRegion
                                          << "buffer content damage" << flushRegion;
            } else {
                m_lastRenderRegion.isFull = true;
                m_lastFlushRegion.isFull = true;
                state.flushRegion = WPixmanRegion(bufferRect);
                state.renderRegion = WPixmanRegion(bufferRect);
                wlr_damage_ring_add_whole(m_damageRing.get());
            }
            if (!isVulkanRhi)
                wd->rhi->finish();
        } else {
            m_lastFlushRegion.region += WPixmanRegion::fromQRegion(softwareRenderer->flushRegion());
            m_lastRenderRegion.region += WPixmanRegion::fromQRegion(softwareRenderer->flushRegion());
            const QSize logicalSize(
                qMax(1, qRound(state.pixelSize.width() / qMax(qreal(1), state.devicePixelRatio))),
                qMax(1, qRound(state.pixelSize.height() / qMax(qreal(1), state.devicePixelRatio))));
            const bool isFull = (!m_lastFlushRegion.region.isEmpty()
                    && (WPixmanRegion(QRect(QPoint(0, 0), logicalSize))
                            - m_lastFlushRegion.region).isEmpty());
            if (isFull) {
                m_lastFlushRegion.isFull = true;
                m_lastRenderRegion.isFull = true;
            }
            state.dirty = WPixmanRegion::fromQRegion(softwareRenderer->flushRegion());
            auto currentImage = getImageFrom(state.renderTarget.rt());
            Q_ASSERT(currentImage && currentImage == softwareRenderer->renderTarget().paintDevice);
            const auto scaleTF = QTransform::fromScale(devicePixelRatio, devicePixelRatio);
            const WPixmanRegion scaledFlushDamage = state.dirty.mappedOuter(scaleTF);
            state.flushRegion += scaledFlushDamage;
            state.renderRegion += scaledFlushDamage;

            {
                if (sourceIndex == 0 && viewportRect.isValid()) {
                    QRect imageRect = (currentImage->operator const QImage &()).rect();
                    WPixmanRegion invalidRegion(imageRect);
                    invalidRegion -= viewportRect;
                    if (!scaledFlushDamage.isEmpty())
                        invalidRegion &= scaledFlushDamage;

                    if (!invalidRegion.isEmpty()) {
                        QPainter pa(currentImage);
                        int count = 0;
                        const pixman_box32_t *rects = invalidRegion.rectangles(&count);
                        for (int i = 0; i < count && rects; ++i) {
                            pa.fillRect(QRect(rects[i].x1, rects[i].y1,
                                              rects[i].x2 - rects[i].x1, rects[i].y2 - rects[i].y1),
                                        softwareRenderer->clearColor());
                        }
                    }
                }
            }

            if (!isRootItem(source.source))
                applyTransform(softwareRenderer, state.worldTransform.inverted().toTransform());
            wlr_damage_ring_add(m_damageRing.get(), scaledFlushDamage);
        }
    }

    if (auto dr = qobject_cast<QSGDefaultRenderContext*>(state.context)) {
        QRhiResourceUpdateBatch *resourceUpdates = wd->rhi->nextResourceUpdateBatch();
        dr->currentFrameCommandBuffer()->resourceUpdate(resourceUpdates);
    }

    if (shouldCacheBuffer())
        wTextureProvider()->setBuffer(state.buffer.get());
}

WDamageRegion WBufferRenderer::endRender()
{
    Q_ASSERT(state.buffer.get());
    {
        WBufferUnlockPtr buffer;
        buffer.swap(state.buffer);
        state.renderer = nullptr;
        state.batchRenderer = nullptr;

        m_lastBuffer = buffer.get();
    }

#ifndef QT_NO_OPENGL
    auto wd = QQuickWindowPrivate::get(window());
    if (state.flags.testFlag(RedirectOpenGLContextDefaultFrameBufferObject)
        && wd->rhi && wd->rhi->backend() == QRhi::OpenGLES2) {
        auto glContext = QOpenGLContext::currentContext();
        Q_ASSERT(glContext);
        QOpenGLContextPrivate::get(glContext)->defaultFboRedirect = GL_NONE;
    }
#endif
    Q_EMIT afterRendering();

    return m_lastRenderRegion;
}
void WBufferRenderer::componentComplete()
{
    QQuickItem::componentComplete();
}

void WBufferRenderer::updateTextureProvider()
{
    if (!m_textureProvider)
        return;

    if (shouldCacheBuffer()) {
        const bool hasCachedBuffer = m_textureProvider->wlrBuffer();
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
    resetSources();
}

void WBufferRenderer::releaseResources()
{
    cleanTextureProvider();
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
    if (isRootItem(d.source)) {
        Q_ASSERT(sourceIndex == 0);
        return QQuickWindowPrivate::get(window())->renderer;
    }

    if (Q_LIKELY(d.renderer))
        return d.renderer;

    auto rootNode = WQmlHelper::getRootNode(d.source);
    Q_ASSERT(rootNode);

    auto dr = qobject_cast<QSGDefaultRenderContext*>(rc);
    const bool useDepth = dr ? dr->useDepthBufferFor2D() : false;
    const auto renderMode = useDepth ? QSGRendererInterface::RenderMode2D
                                     : QSGRendererInterface::RenderMode2DNoDepthBuffer;
    d.renderer = rc->createRenderer(renderMode);
    // refFromEffectItem provides this Qt-owned root and its dirty notifications.
    // Borrow it without reparenting nodes; only the renderer and its separate
    // damage tree belong to this source.
    d.renderer->setRootNode(rootNode);
    QObject::connect(d.renderer, &QSGRenderer::sceneGraphChanged,
                     this, &WBufferRenderer::sceneGraphChanged);

    d.renderer->setClearColor(m_clearColor);

    return d.renderer;
}

void WBufferRenderer::ensureRotateBuffer()
{
    if (!state.needsRotateBuffer)
        return;
    state.needsRotateBuffer = false;
    state.dirty.clear();
    wlr_damage_ring_rotate_buffer(m_damageRing.get(), state.buffer.get(), state.dirty);

    auto rtValue = state.renderTarget.rt();
    auto rtd = QQuickRenderTargetPrivate::get(&rtValue);
    if (rtd && rtd->type == QQuickRenderTargetPrivate::Type::PaintDevice) {
        if (state.devicePixelRatio != 1.0) {
            state.dirty = state.dirty.mappedOuter(
                QTransform::fromScale(1.0 / state.devicePixelRatio, 1.0 / state.devicePixelRatio));
        }
    }
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wbufferrenderer_p.cpp"
