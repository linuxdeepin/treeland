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
        if (m_texture)
            cleanTexture();
    }

    QSGTexture *texture() const override {
        return m_texture ? &m_texture->texture : nullptr;
    }

    void setBuffer(QWBuffer *buffer) {
        Q_ASSERT(item);

        if (m_texture)
            cleanTexture();

        Q_ASSERT(!m_texture);
        if (buffer)
            m_texture = new Texture(item->window(), item->output()->renderer(), buffer);

        Q_EMIT textureChanged();
    }

    void cleanTexture() {
        Q_ASSERT(item);

        class TextureCleanupJob : public QRunnable
        {
        public:
            TextureCleanupJob(Texture *texture)
                : texture(texture) { }
            void run() override {
                delete texture;
            }
            Texture *texture;
        };

        // Delay clean the textures on the next render after.
        item->window()->scheduleRenderJob(new TextureCleanupJob(m_texture),
                                          QQuickWindow::AfterSynchronizingStage);
        m_texture = nullptr;
    }

    void invalidate() {
        cleanTexture();
        item = nullptr;

        Q_EMIT textureChanged();
    }

    WBufferRenderer *item;

    struct Texture {
        Texture(QQuickWindow *window, QWRenderer *renderer, QWBuffer *buffer)
        {
            qwtexture = QWTexture::fromBuffer(renderer, buffer);
            WTexture::makeTexture(qwtexture, &texture, window);
            texture.setOwnsTexture(false);
        }

        ~Texture() {
            delete qwtexture;
        }

        QWTexture *qwtexture;
        QSGPlainTexture texture;
    };

    Texture *m_texture = nullptr;
};

WBufferRenderer::WBufferRenderer(QQuickItem *parent)
    : QQuickItem(parent)
    , m_cacheBuffer(false)
    , m_forceCacheBuffer(false)
    , m_hideSource(false)
{
    m_textureProvider.reset(new TextureProvider(this));
}

WBufferRenderer::~WBufferRenderer()
{
    resetSources();

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

int WBufferRenderer::sourceCount() const
{
    return m_sourceList.size();
}

QList<QQuickItem*> WBufferRenderer::sourceList() const
{
    QList<QQuickItem*> list;
    list.reserve(m_sourceList.size());

    for (const Data &i : m_sourceList)
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
    m_sourceList.clear();
    m_hideSource = hideSource;

    for (auto s : sources) {
        m_sourceList.append({s, nullptr});

        if (isRootItem(s))
            continue;

        connect(s, &QQuickItem::destroyed, this, [this] {
            const int index = indexOfSource(static_cast<QQuickItem*>(sender()));
            Q_ASSERT(index >= 0);
            removeSource(index);
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

QWBuffer *WBufferRenderer::currentBuffer() const
{
    return state.buffer;
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
    return true;
}

QSGTextureProvider *WBufferRenderer::textureProvider() const
{
    return m_textureProvider.get();
}

QWBuffer *WBufferRenderer::beginRender(const QSize &pixelSize, qreal devicePixelRatio,
                                       uint32_t format, RenderFlags flags)
{
    Q_ASSERT(!state.buffer);
    Q_ASSERT(m_output);

    m_damageRing.setBounds(pixelSize);
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
        bool ok = m_output->configureSwapchain(pixelSize, format, &m_swapchain, !flags.testFlag(DontTestSwapchain));
        if (!ok)
            return nullptr;
    }

    if (!shouldCacheBuffer())
        m_textureProvider->setBuffer(nullptr);

    // TODO: Support scanout buffer of wlr_surface(from WSurfaceItem)
    int bufferAge;
    auto buffer = m_swapchain->acquire(&bufferAge);
    if (!buffer)
        return nullptr;

    if (!m_renderHelper)
        m_renderHelper = new WRenderHelper(m_output->renderer());
    m_renderHelper->setSize(pixelSize);

    auto wd = QQuickWindowPrivate::get(window());
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

#ifndef QT_NO_OPENGL
        if (wd->rhi->backend() == QRhi::OpenGLES2) {
            auto glRT = QRHI_RES(QGles2TextureRenderTarget, rtd->u.rhiRt);
            Q_ASSERT(glRT->framebuffer > 0);
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
    state.bufferAge = bufferAge;
    state.lastRT = lastRT;
    state.buffer = buffer;
    state.renderTarget = rt;
    state.sgRenderTarget = sgRT;

    if (!shouldCacheBuffer())
        m_textureProvider->setBuffer(buffer);

    return buffer;
}

void WBufferRenderer::render(int sourceIndex, QMatrix4x4 renderMatrix, bool preserveColorContents)
{
    Q_ASSERT(state.buffer);

    const auto &source = m_sourceList.at(sourceIndex);
    QSGRenderer *renderer = ensureRenderer(source.source, state.context);
    auto wd = QQuickWindowPrivate::get(window());

    renderer->setDevicePixelRatio(state.devicePixelRatio);
    renderer->setDeviceRect(QRect(QPoint(0, 0), state.pixelSize));
    renderer->setViewportRect(state.pixelSize);
    renderer->setRenderTarget(state.sgRenderTarget);

    auto softwareRenderer = dynamic_cast<QSGSoftwareRenderer*>(renderer);
    { // before render
        if (softwareRenderer) {
            auto image = getImageFrom(state.renderTarget);
            image->setDevicePixelRatio(state.devicePixelRatio);

            // TODO: Should set to QSGSoftwareRenderer, but it's not support specify matrix.
            // If transform is changed, it will full repaint.
            if (isRootItem(source.source)) {
                auto rootTransform = QQuickItemPrivate::get(wd->contentItem)->itemNode();
                if (rootTransform->matrix() != renderMatrix)
                    rootTransform->setMatrix(renderMatrix);
            } else {
                auto t = renderMatrix.toTransform();
                if (t.type() > QTransform::TxTranslate) {
                    (image->operator QImage &()).fill(renderer->clearColor());
                    softwareRenderer->markDirty();
                }

                applyTransform(softwareRenderer, t);
            }
        } else {
            bool flipY = wd->rhi ? !wd->rhi->isYUpInNDC() : false;
            if (state.renderTarget.mirrorVertically())
                flipY = !flipY;

            QRectF rect(QPointF(0, 0), QSizeF(state.pixelSize) / state.devicePixelRatio);

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

            auto textureRT = static_cast<QRhiTextureRenderTarget*>(state.sgRenderTarget.rt);
            if (preserveColorContents) {
                textureRT->setFlags(textureRT->flags() | QRhiTextureRenderTarget::PreserveColorContents);
            } else {
                textureRT->setFlags(textureRT->flags() & ~QRhiTextureRenderTarget::PreserveColorContents);
            }
        }
    }

    state.context->renderNextFrame(renderer);

    { // after render
        if (!softwareRenderer) {
            // TODO: get damage area from QRhi renderer
            m_damageRing.addWhole();
            // ###: maybe Qt bug? Before executing QRhi::endOffscreenFrame, we may
            // use the same QSGRenderer for multiple drawings. This can lead to
            // rendering the same content for different QSGRhiRenderTarget instances
            // when using the RhiGles backend. Additionally, considering that the
            // result of the current drawing may be needed when drawing the next
            // sourceIndex, we should let the RHI (Rendering Hardware Interface)
            // complete the results of this drawing here to ensure the current
            // drawing result is available for use.
            wd->rhi->finish();
        } else {
            auto currentImage = getImageFrom(state.renderTarget);
            Q_ASSERT(currentImage && currentImage == softwareRenderer->m_rt.paintDevice);
            currentImage->setDevicePixelRatio(1.0);
            const auto scaleTF = QTransform::fromScale(state.devicePixelRatio, state.devicePixelRatio);
            const auto scaledFlushRegion = scaleTF.map(softwareRenderer->flushRegion());
            PixmanRegion scaledFlushDamage;
            bool ok = WTools::toPixmanRegion(scaledFlushRegion, scaledFlushDamage);
            Q_ASSERT(ok);

            {
                PixmanRegion damage;
                m_damageRing.getBufferDamage(state.bufferAge, damage);

                if (!damage.isEmpty() && state.lastRT.first != state.buffer && !state.lastRT.second.isNull()) {
                    auto image = getImageFrom(state.lastRT.second);
                    Q_ASSERT(image);
                    Q_ASSERT(image->size() == state.pixelSize);

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

            if (!isRootItem(source.source))
                applyTransform(softwareRenderer, renderMatrix.inverted().toTransform());
            m_damageRing.add(scaledFlushDamage);
        }
    }

    if (auto dr = qobject_cast<QSGDefaultRenderContext*>(state.context)) {
        QRhiResourceUpdateBatch *resourceUpdates = wd->rhi->nextResourceUpdateBatch();
        dr->currentFrameCommandBuffer()->resourceUpdate(resourceUpdates);
    }
}

void WBufferRenderer::endRender()
{
    Q_ASSERT(state.buffer);
    auto buffer = state.buffer;
    state.buffer = nullptr;
    if (shouldCacheBuffer())
        m_textureProvider->setBuffer(buffer);

    m_lastBuffer = buffer;
    m_damageRing.rotate();
    m_swapchain->setBufferSubmitted(buffer);
    buffer->unlock();

#ifndef QT_NO_OPENGL
    auto wd = QQuickWindowPrivate::get(window());
    if (state.flags.testFlag(RedirectOpenGLContextDefaultFrameBufferObject)
        && wd->rhi && wd->rhi->backend() == QRhi::OpenGLES2) {
        auto glContext = QOpenGLContext::currentContext();
        Q_ASSERT(glContext);
        QOpenGLContextPrivate::get(glContext)->defaultFboRedirect = GL_NONE;
    }
#endif
}

void WBufferRenderer::componentComplete()
{
    QQuickItem::componentComplete();
}

void WBufferRenderer::setForceCacheBuffer(bool force)
{
    if (m_forceCacheBuffer == force)
        return;
    m_forceCacheBuffer = force;
    updateTextureProvider();
}

void WBufferRenderer::resetTextureProvider()
{
    if (m_textureProvider)
        m_textureProvider->setBuffer(nullptr);
}

void WBufferRenderer::updateTextureProvider()
{
    if (shouldCacheBuffer()) {
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

        m_textureProvider->invalidate();
        // Delay clean the textures on the next render after.
        window()->scheduleRenderJob(new TextureProviderCleanupJob(m_textureProvider.release()),
                                    QQuickWindow::AfterRenderingStage);
    }
}

void WBufferRenderer::resetSources()
{
    for (int i = 0; i < m_sourceList.size(); ++i) {
        removeSource(i);
    }
}

void WBufferRenderer::removeSource(int index)
{
    auto s = m_sourceList.at(index);
    if (isRootItem(s.source))
        return;

    s.renderer->deleteLater();
    auto d = QQuickItemPrivate::get(s.source);
    if (d->inDestructor)
        return;

    s.source->disconnect(this);
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

QSGRenderer *WBufferRenderer::ensureRenderer(QQuickItem *source, QSGRenderContext *rc)
{
    if (isRootItem(source))
        return QQuickWindowPrivate::get(window())->renderer;

    const int index = indexOfSource(source);
    Q_ASSERT(index >= 0);
    Data &d = m_sourceList[index];

    if (Q_LIKELY(d.renderer))
        return d.renderer;

    auto rootNode = getRootNode(source);
    Q_ASSERT(rootNode);

    auto dr = qobject_cast<QSGDefaultRenderContext*>(rc);
    const bool useDepth = dr ? dr->useDepthBufferFor2D() : false;
    const auto renderMode = useDepth ? QSGRendererInterface::RenderMode2D
                                     : QSGRendererInterface::RenderMode2DNoDepthBuffer;
    d.renderer = rc->createRenderer(renderMode);
    d.renderer->setRootNode(rootNode);
    QObject::connect(d.renderer, &QSGRenderer::sceneGraphChanged,
                     this, &WBufferRenderer::sceneGraphChanged);

    d.renderer->setClearColor(Qt::transparent);

    return d.renderer;
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wbufferrenderer_p.cpp"
