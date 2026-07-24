// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QDebug>

#include "wrenderhelper.h"
#include "wayliblogging.h"
#include "wrenderbuffernode_p.h"
#include "wbufferrenderer_p.h"
#include "wqmlhelper_p.h"
#include "platformplugin/types.h"
#include "private/wprivateaccessor_p.h"
#include "utils/private/wvulkantrace_p.h"

#include <qwtexture.h>

#include <QQuickItem>
#include <QRunnable>
#include <QScopeGuard>
#include <QSGImageNode>
#include <QVulkanFunctions>
#include <private/qquickitem_p.h>
#include <private/qsgplaintexture_p.h>
#include <private/qrhi_p.h>
#include <private/qrhivulkan_p.h>
#include <private/qsgrenderer_p.h>
#include <private/qsgbatchrenderer_p.h>
#include <private/qsgdefaultrendercontext_p.h>
#include <private/qsgrhisupport_p.h>
#include <private/qquickrendercontrol_p.h>
#include <qobjectdefs.h>

#include <algorithm>

// QSGRenderer: access protected render stages and private bit-field members
// m_changed_emitted/m_is_rendering.
//
// preprocess() and render() are protected virtual functions — the template
// accessor [temp.explicit]/12 trick legally bypasses access control here too.
//
// m_changed_emitted and m_is_rendering are private bit fields packed into a
// single uint that immediately follows m_nodes_dont_preprocess. Use
// W_DECLARE_PRIVATE_BITFIELD to reach that storage unit via memory arithmetic.
//   Declaration order → bit position (GCC/Clang little-endian LSB-first):
//     m_changed_emitted : 1  → bit 0
//     m_is_rendering    : 1  → bit 1
W_DECLARE_PRIVATE_METHOD(QSGRenderer_preprocess_tag, QSGRenderer, preprocess, void);
W_DECLARE_PRIVATE_METHOD(QSGRenderer_render_tag, QSGRenderer, render, void);
W_DECLARE_PRIVATE_BITFIELD(QSGRenderer_m_nodes_dont_preprocess_tag,
                           QSGRenderer, m_nodes_dont_preprocess,
                           QSet<QSGNode *>, uint);
static constexpr unsigned k_m_changed_emitted_bit = 0;
static constexpr unsigned k_m_is_rendering_bit    = 1;

// Compile-time layout verification.
//
// W_DECLARE_PRIVATE_BITFIELD already checks at compile time that
// m_nodes_dont_preprocess exists in QSGRenderer with the correct type.
// What it cannot verify is whether the bit fields still immediately follow
// that member in the class layout.
//
// Since the bit fields are the last members of QSGRenderer (Qt 6.11.1,
// qsgrenderer_p.h:137–144), any insertion or reordering of members changes
// sizeof(QSGRenderer).  We assert the known size as a layout fingerprint.
// If this fires, diff qsgrenderer_p.h and update the accessor + constant below.
//
// Verified: Qt 6.11.1, x86_64 Linux.
#if defined(Q_PROCESSOR_X86_64) && (defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD))
static_assert(sizeof(QSGRenderer) == 432,
    "QSGRenderer size changed — review qsgrenderer_p.h and update the bit-field accessor");
#endif

// QSGBatchRenderer::Renderer::m_shaderManager is a private data member.
// QSGBatchRenderer::Renderer::useDepthBuffer() is a private const method.
// Access them via the explicit-instantiation template trick.
W_DECLARE_PRIVATE_MEMBER(QSGBatchRenderer_m_shaderManager_tag,
                         QSGBatchRenderer::Renderer, m_shaderManager,
                         QSGBatchRenderer::ShaderManager*);
W_DECLARE_PRIVATE_CONST_METHOD(QSGBatchRenderer_useDepthBuffer_tag,
                                QSGBatchRenderer::Renderer, useDepthBuffer, bool);

static inline QSGBatchRenderer::ShaderManager *&rendererShaderManager(QSGBatchRenderer::Renderer *r) {
    return W_PRIVATE_MEMBER(*r, QSGBatchRenderer_m_shaderManager_tag{});
}
static inline bool rendererUseDepthBuffer(const QSGBatchRenderer::Renderer *r) {
    return W_PRIVATE_CALL(*r, QSGBatchRenderer_useDepthBuffer_tag{});
}
static inline void rendererPreprocess(QSGRenderer *r) {
    W_PRIVATE_CALL(*r, QSGRenderer_preprocess_tag{});
}
static inline void rendererRender(QSGRenderer *r) {
    W_PRIVATE_CALL(*r, QSGRenderer_render_tag{});
}

WAYLIB_SERVER_BEGIN_NAMESPACE

static bool recordVulkanDepthStencilPassBarrier(QRhi *rhi,
                                                QRhiCommandBuffer *commandBuffer,
                                                QRhiTexture *depthStencilTexture)
{
    if (!rhi || rhi->backend() != QRhi::Vulkan || !commandBuffer
        || !depthStencilTexture) {
        return false;
    }

    const auto *nativeHandles = static_cast<const QRhiVulkanNativeHandles *>(
        rhi->nativeHandles());
    auto *vkTexture = static_cast<QVkTexture *>(depthStencilTexture);
    if (!nativeHandles || !nativeHandles->inst
        || nativeHandles->dev == VK_NULL_HANDLE
        || vkTexture->image == VK_NULL_HANDLE) {
        return false;
    }

    QVulkanDeviceFunctions *deviceFunctions =
        nativeHandles->inst->deviceFunctions(nativeHandles->dev);
    if (!deviceFunctions)
        return false;

    commandBuffer->beginExternal();
    const auto *commandBufferHandles =
        static_cast<const QRhiVulkanCommandBufferNativeHandles *>(
            commandBuffer->nativeHandles());
    if (!commandBufferHandles
        || commandBufferHandles->commandBuffer == VK_NULL_HANDLE) {
        commandBuffer->endExternal();
        return false;
    }

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vkTexture->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
        | VK_IMAGE_ASPECT_STENCIL_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    constexpr VkPipelineStageFlags depthStencilStages =
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
        | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deviceFunctions->vkCmdPipelineBarrier(
        commandBufferHandles->commandBuffer,
        depthStencilStages,
        depthStencilStages,
        VK_DEPENDENCY_BY_REGION_BIT,
        0, nullptr,
        0, nullptr,
        1, &barrier);
    commandBuffer->endExternal();
    return true;
}

class Q_DECL_HIDDEN DataManagerBase : public QObject
{
public:
    mutable QAtomicInt ref;

    explicit DataManagerBase(QQuickWindow *owner)
        : QObject(owner)
    {
        Q_ASSERT(owner->isSceneGraphInitialized());
        connect(owner, &QQuickWindow::sceneGraphInvalidated, this, [this]() {
            setParent(nullptr);
            // per request from zccrs.
            // Be Warned: objects may not be expected to be deleted in the rendering thread.
            delete this;
        }, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::SingleShotConnection));
    }
    virtual ~DataManagerBase() {};
};

template <class T>
class Q_DECL_HIDDEN DataManagerPointer
{
    static_assert(std::is_base_of<DataManagerBase, T>::value);
public:
    DataManagerPointer() noexcept = default;
    constexpr DataManagerPointer(std::nullptr_t) noexcept : DataManagerPointer{} {}
    inline DataManagerPointer(T *p) : pointer(p) {
        if (pointer)
            pointer->ref.ref();
    }

    DataManagerPointer(DataManagerPointer<T> &&other) noexcept
        : pointer(std::exchange(other.pointer, nullptr)) {}

    DataManagerPointer(const DataManagerPointer<T> &other) noexcept
        : pointer(other.pointer) {
        ref();
    }

    DataManagerPointer &operator=(const DataManagerPointer<T> &other) noexcept
    {
        if (this == &other)
            return *this;

        deref();
        pointer = other.pointer;
        ref();
        return *this;
    }

    DataManagerPointer &operator=(DataManagerPointer<T> &&other) noexcept
    {
        if (this == &other)
            return *this;

        deref();
        pointer = std::exchange(other.pointer, nullptr);
        return *this;
    }

    ~DataManagerPointer() {
        deref();
    }

    inline DataManagerPointer<T> &operator=(T* p) {
        if (pointer == p)
            return *this;

        deref();
        pointer = p;
        ref();
        return *this;
    }

    T* data() const noexcept
    { return pointer; }
    T* get() const noexcept
    { return data(); }
    T* operator->() const noexcept
    { return data(); }
    T& operator*() const noexcept
    { return *data(); }
    operator T*() const noexcept
    { return data(); }

    bool isNull() const noexcept
    { return pointer.isNull(); }

    bool operator==(T *other) const noexcept
    { return pointer == other; }
    bool operator!=(T *other) const noexcept
    { return pointer != other; }

private:
    void deref() {
        if (!pointer)
            return;
        pointer->ref.deref();
        if (pointer->ref == 0) {
            pointer->DataManagerBase::deleteLater();
            pointer.clear();
        }
    }

    void ref() {
        if (pointer)
            pointer->ref.ref();
    }

    QPointer<T> pointer;
};

template <class Derive, class DataType, typename... DataKeys>
class Q_DECL_HIDDEN DataManager : public DataManagerBase
{
public:
    struct Data {
        int released = 0;
        DataType *data = nullptr;
    };

    static DataManagerPointer<Derive> get(QQuickWindow *owner) {
        return owner->findChild<Derive*>({}, Qt::FindDirectChildrenOnly);
    }

    static DataManagerPointer<Derive> resolveByOwner(const DataManagerPointer<Derive> &other, QQuickWindow *owner) {
        static_assert(QtPrivate::HasQ_OBJECT_Macro<Derive>::Value, "Derive must have Q_OBJECT macro");
        Q_ASSERT(owner);
        if (other && other->owner() == owner)
            return other;
        {
            Derive *other = get(owner);
            if (!other)
                other = new Derive(owner);
            return other;
        }
    }

    inline QQuickWindow *owner() const {
        return static_cast<QQuickWindow*>(parent());
    }

    std::weak_ptr<Data> resolve(std::weak_ptr<Data> data, DataKeys&&... keys) {
        struct TryClean {
            TryClean(DataManager *m)
                : manager(m) {}
            ~TryClean() {
                manager->tryClean();
            }
            DataManager *manager;
        };

        [[maybe_unused]] TryClean cleanJob(this);

        {
            auto d = data.lock();
            if (d && dataList.contains(d)) {
                if (get()->check(d->data, std::forward<DataKeys>(keys)...)) {
                    d->released = 0;
                    return data;
                }
                release(data);
            }
        }

        for (auto data : std::as_const(dataList)) {
            if (get()->check(data->data, std::forward<DataKeys>(keys)...)) {
                data->released = 0;
                return data;
            }
        }

        auto newData = std::make_shared<Data>();
        newData->data = get()->create(std::forward<DataKeys>(keys)...);
        if (newData->data) {
            dataList.append(newData);
            return newData;
        }

        return {};
    }

    inline void release(std::weak_ptr<Data> data) {
        auto d = data.lock();
        if (!d)
            return;
        d->released++;
    }

protected:
    struct CleanJob : public QRunnable {
        CleanJob(DataManager *manager)
            : manager(manager) {}

        void run() override {
            if (!manager)
                return;

            manager->cleanJob = nullptr;

            // Collect items to destroy outside the manager's dataList
            // to avoid accessing manager during shared_ptr destruction
            QList<DataType*> itemsToDestroy;
            QList<std::shared_ptr<Data>> tmp;
            tmp.swap(manager->dataList);
            manager->dataList.reserve(tmp.size());

            for (const auto &data : std::as_const(tmp)) {
                if (data->released > 2) {
                    // Collect items to destroy instead of destroying immediately
                    itemsToDestroy.append(data->data);
                } else {
                    manager->dataList << data;

                    if (data->released > 0)
                        ++data->released;
                }
            }

            // Destroy items after we're done accessing manager
            // This prevents crashes from shared_ptr destruction during dataList iteration
            for (auto item : std::as_const(itemsToDestroy)) {
                manager->get()->destroy(item);
            }
        }

        QPointer<DataManager> manager;
    };

    inline void tryClean() {
        if (Q_LIKELY(!cleanJob)) {
            cleanJob = new CleanJob(this);
            owner()->scheduleRenderJob(cleanJob, QQuickWindow::AfterRenderingStage);
        }
    }

    inline const Derive *get() const {
        return static_cast<const Derive*>(this);
    }

    inline Derive *get() {
        return static_cast<Derive*>(this);
    }

    using QObject::deleteLater;
    ~DataManager() override {
        for (auto data : std::as_const(dataList)) {
            Derive::destroy(data->data);
        }
    }

private:
    friend Derive;

    DataManager(QQuickWindow *owner)
        : DataManagerBase(owner) {
        Q_ASSERT(owner->findChildren<Derive*>(Qt::FindDirectChildrenOnly).size() == 0);
    }

protected:

    QList<std::shared_ptr<Data>> dataList;
    QRunnable *cleanJob = nullptr;
};

struct WlrAndRhiTexture {
    QRhiTexture *rhiTexture = nullptr;
    uint32_t drmFormat = 0;
    uint64_t drmModifier = 0;
    QRhiTexture::Flags flags;
};

class Q_DECL_HIDDEN RhiTextureManager : public DataManager<RhiTextureManager, WlrAndRhiTexture, QRhiTexture::Format, uint32_t, uint64_t, const QSize&, QRhiTexture::Flags>
{
    Q_OBJECT

    friend class DataManager;

    RhiTextureManager(QQuickWindow *owner)
        : DataManager<RhiTextureManager, WlrAndRhiTexture, QRhiTexture::Format,
                      uint32_t, uint64_t, const QSize&, QRhiTexture::Flags>(owner) {
        Q_ASSERT(owner->findChildren<RhiTextureManager*>(Qt::FindDirectChildrenOnly).size() == 1);
    }

    static bool check(WlrAndRhiTexture *texture, QRhiTexture::Format,
                      uint32_t drmFormat, uint64_t drmModifier,
                      const QSize &size, QRhiTexture::Flags flags) {
        return texture->drmFormat == drmFormat
            && texture->drmModifier == drmModifier
            && texture->rhiTexture
            && texture->rhiTexture->pixelSize() == size
            && texture->flags == flags;
    }

    WlrAndRhiTexture *create(QRhiTexture::Format format,
                             uint32_t drmFormat, uint64_t drmModifier,
                             const QSize &size, QRhiTexture::Flags flags) {
        auto texture = owner()->rhi()->newTexture(format, size, 1,
                                                  QRhiTexture::RenderTarget | flags);
        texture->setName(QByteArrayLiteral("WaylibRenderBufferNodeTexture"));
        if (!texture->create()) {
            qCWarning(lcWlRenderBuffer) << "Failed to create QRhi texture for WRenderBufferNode"
                                        << "format" << format
                                        << "drmFormat" << drmFormat
                                        << "modifier" << drmModifier
                                        << "size" << size;
            delete texture;
            return nullptr;
        }

        return new WlrAndRhiTexture{texture, drmFormat, drmModifier, flags};
    }

    static void destroy(WlrAndRhiTexture *texture) {
        delete texture->rhiTexture;
        delete texture;
    }
};

class Q_DECL_HIDDEN RhiManager : public DataManager<RhiManager, void>
{
    Q_OBJECT
public:
    QRhi *rhi() const {
        return m_rhi->rhi;
    }

    QQuickGraphicsConfiguration graphicsConfiguration() const {
        return m_rhi->gc;
    }

    void sync(const QSize &pixelSize, QSGRootNode *rootNode,
              const QMatrix4x4 &matrix = {}, const QMatrix4x4 &baseProjectionMatrix = {},
              QSGRenderer *base = nullptr, const QVector2D &dpr = {}) {
        Q_ASSERT(!renderer->rootNode());

        if (base) {
            renderer->setDevicePixelRatio(base->devicePixelRatio());
            renderer->setDeviceRect(base->deviceRect());
            renderer->setViewportRect(base->viewportRect());

            // The m22 and m23 is control the z-order for depth test.
            // If have a base QSGRenderer, we should inherit the depth test from
            // baseProjectionMatrix.
            renderer->setProjectionMatrix(baseProjectionMatrix);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            renderer->setProjectionMatrixWithNativeNDC(base->projectionMatrixWithNativeNDC(0));
#else
            renderer->setProjectionMatrixWithNativeNDC(base->projectionMatrixWithNativeNDC());
#endif
        } else {
            renderer->setDevicePixelRatio(1.0);
            renderer->setDeviceRect(QRect(QPoint(0, 0), pixelSize));
            renderer->setViewportRect(pixelSize);

            QRectF rect(QPointF(0, 0), QSizeF(pixelSize.width() / dpr.x(),
                                              pixelSize.height() / dpr.y()));
            renderer->setProjectionMatrixToRect(rect, rhi()->isYUpInNDC()
                                                          ? QSGRenderer::MatrixTransformFlipY
                                                          : QSGRenderer::MatrixTransformFlag {});
        }

        if (Q_UNLIKELY(!matrix.isIdentity())) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            renderer->setProjectionMatrix(renderer->projectionMatrix(0) * matrix);
            renderer->setProjectionMatrixWithNativeNDC(renderer->projectionMatrixWithNativeNDC(0) * matrix);
#else
            renderer->setProjectionMatrix(renderer->projectionMatrix() * matrix);
            renderer->setProjectionMatrixWithNativeNDC(renderer->projectionMatrixWithNativeNDC() * matrix);
#endif
        }

        renderer->setRootNode(rootNode);
    }

    bool preprocess(QRhiRenderTarget *rt, qreal &oldDPR, QRhiCommandBuffer* &oldCB) {
        QRhiCommandBuffer *cb = nullptr;
        if (rhi()->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
            return false;
        Q_ASSERT(cb);

        renderer->setRenderTarget({ rt, rt->renderPassDescriptor(), cb });
        auto dc = static_cast<QSGDefaultRenderContext*>(context);
        oldDPR = dc->currentDevicePixelRatio();
        oldCB = dc->currentFrameCommandBuffer();
        context->prepareSync(renderer->devicePixelRatio(), cb, graphicsConfiguration());

        W_PRIVATE_BF_SET(*renderer, QSGRenderer_m_nodes_dont_preprocess_tag, k_m_is_rendering_bit, true);
        rendererPreprocess(renderer);

        return true;
    }

    bool render(qreal oldDPR, QRhiCommandBuffer* &oldCB, bool forceDepthTest = false) {
        Q_ASSERT(W_PRIVATE_BF_GET(*renderer, QSGRenderer_m_nodes_dont_preprocess_tag, k_m_is_rendering_bit));
        rendererRender(renderer);
        W_PRIVATE_BF_SET(*renderer, QSGRenderer_m_nodes_dont_preprocess_tag, k_m_is_rendering_bit, false);
        W_PRIVATE_BF_SET(*renderer, QSGRenderer_m_nodes_dont_preprocess_tag, k_m_changed_emitted_bit, false);

        context->prepareSync(oldDPR, oldCB, graphicsConfiguration());
        bool ok = false;

        do {
            if (forceDepthTest && Q_LIKELY(isBatchRenderer)) {
                auto batchRenderer = static_cast<QSGBatchRenderer::Renderer*>(renderer);
                if (auto sm = rendererShaderManager(batchRenderer)) {
                    if (Q_LIKELY(!sm->pipelineCache.isEmpty())) {
                        QVector<std::pair<QRhiGraphicsPipeline*, bool>> tmp;
                        tmp.reserve(sm->pipelineCache.size());

                        for (auto pipeline : std::as_const(sm->pipelineCache)) {
                            tmp.append(std::make_pair(pipeline, pipeline->hasDepthTest()));
                            pipeline->setDepthTest(true);
                        }
                        ok = rhi()->endOffscreenFrame() == QRhi::FrameOpSuccess;
                        Q_ASSERT(tmp.size() == sm->pipelineCache.size());
                        for (auto i : std::as_const(tmp))
                            i.first->setDepthTest(i.second);

                        break;
                    }
                }
            }

            ok = rhi()->endOffscreenFrame() == QRhi::FrameOpSuccess;
        } while (false);

        renderer->setRootNode(nullptr);

        return ok;
    }

    inline bool render(QRhiRenderTarget *rt, bool forceDepthTest = false) {
        qreal oldDPR;
        QRhiCommandBuffer *oldCB;

        if (!preprocess(rt, oldDPR, oldCB))
            return false;

        return render(oldDPR, oldCB, forceDepthTest);
    }

private:
    friend class DataManager;

    RhiManager(QQuickWindow *owner)
        : DataManager<RhiManager, void>(owner) {
        Q_ASSERT(owner->findChildren<RhiManager*>(Qt::FindDirectChildrenOnly).size() == 1);
        if (owner->rhi()->backend() == QRhi::Vulkan) {
            m_rhi.reset(new Rhi());
            m_rhi->rhi = owner->rhi();
            m_rhi->own = false;
            m_rhi->gc = owner->graphicsConfiguration();
        } else {
            std::unique_ptr<QOffscreenSurface> fallbackSurface(new QW::OffscreenSurface(nullptr));
            fallbackSurface->create();

            auto rhi = QSGRhiSupport::instance()->createRhi(owner, fallbackSurface.get());
            if (!rhi.rhi)
                return;
            Q_ASSERT(rhi.rhi->backend() == owner->rhi()->backend());
            m_rhi.reset(new Rhi());
            m_rhi->rhi = rhi.rhi;
            m_rhi->own = rhi.own;
            m_rhi->gc = owner->graphicsConfiguration();
            m_rhi->offscreenSurface = fallbackSurface.release();
        }

        context = QQuickWindowPrivate::get(owner)->context;
        // Don't use RenderMode2D, when use this renderer on an exists renderTarget
        // we need to ensure the renderer don't overwrite the depth buffer, because
        // the exists renderTarget is using in the other QSGRenderer, maybe that
        // renderer is using depth test, and the other QSGRenderer is not finished render.
        // For an example: RhiNode to render its content nodes on an exists renderTarget.
        renderer = context->createRenderer(QSGRendererInterface::RenderMode2DNoDepthBuffer);
        isBatchRenderer = dynamic_cast<QSGBatchRenderer::Renderer*>(renderer);
    }

    ~RhiManager() override {
        delete renderer;
    }

    static bool check() {
        Q_UNREACHABLE();
        return true;
    }

    static void *create() {
        Q_UNREACHABLE();
        return nullptr;
    }

    static void destroy(void*) {
        Q_UNREACHABLE();
    }

    struct Rhi {
        QRhi *rhi = nullptr;
        QOffscreenSurface *offscreenSurface = nullptr;
        bool own = false;
        QQuickGraphicsConfiguration gc;

        ~Rhi() {
            if (!own)
                return;

            auto rhiSupport = QSGRhiSupport::instance();
            if (rhiSupport)
                rhiSupport->destroyRhi(rhi, gc);
            else
                delete rhi;

            delete offscreenSurface;
        }
    };

    QSGRenderContext *context;
    QSGRenderer *renderer;
    bool isBatchRenderer = false;

    QScopedPointer<Rhi> m_rhi;
};

// A QSG batch renderer owns its Elements and their shader-resource bindings.
// Reusing one renderer for multiple blitter roots in the same Vulkan command
// buffer lets Qt recycle and update a descriptor set that an earlier root has
// already bound. Keep one renderer per blitter role instead. Qt's render
// context still shares shader and pipeline caches between these renderers.
class Q_DECL_HIDDEN VulkanInlineRenderer
{
public:
    VulkanInlineRenderer(QQuickWindow *owner, const void *blitterNode,
                         const char *role)
        : m_owner(owner)
        , m_blitterNode(blitterNode)
        , m_role(role)
    {
        if (!owner || !owner->rhi()
            || owner->rhi()->backend() != QRhi::Vulkan) {
            return;
        }

        m_rhi = owner->rhi();
        m_graphicsConfiguration = owner->graphicsConfiguration();
        m_context = QQuickWindowPrivate::get(owner)->context;
        if (!m_context)
            return;

        // RenderMode2DNoDepthBuffer matches the existing RhiManager path: the
        // surrounding compositor renderer owns the shared depth attachment.
        m_renderer = m_context->createRenderer(
            QSGRendererInterface::RenderMode2DNoDepthBuffer);

        if (m_renderer && WVulkanTrace::enabled()) {
            qCDebug(lcWlRenderBuffer).noquote()
                << QStringLiteral("VKTRACE event=blitter-inline-renderer-create node=%1 role=%2 renderer=%3")
                       .arg(quintptr(m_blitterNode), 0, 16)
                       .arg(QString::fromLatin1(m_role))
                       .arg(quintptr(m_renderer), 0, 16);
        }
    }

    ~VulkanInlineRenderer()
    {
        if (!m_renderer)
            return;

        Q_ASSERT(!m_prepared);
        m_renderer->setRootNode(nullptr);
        if (WVulkanTrace::enabled()) {
            qCDebug(lcWlRenderBuffer).noquote()
                << QStringLiteral("VKTRACE event=blitter-inline-renderer-destroy node=%1 role=%2 renderer=%3")
                       .arg(quintptr(m_blitterNode), 0, 16)
                       .arg(QString::fromLatin1(m_role))
                       .arg(quintptr(m_renderer), 0, 16);
        }
        delete m_renderer;
    }

    bool isValid() const
    {
        return m_renderer && m_rhi && m_context;
    }

    QQuickWindow *owner() const
    {
        return m_owner;
    }

    QSGRenderer *renderer() const
    {
        return m_renderer;
    }

    QSGRootNode *rootNode() const
    {
        return m_renderer ? m_renderer->rootNode() : nullptr;
    }

    bool sync(const QSize &pixelSize, QSGRootNode *rootNode,
              const QMatrix4x4 &matrix = {},
              const QMatrix4x4 &baseProjectionMatrix = {},
              QSGRenderer *base = nullptr, const QVector2D &dpr = {})
    {
        if (!isValid() || !rootNode || m_prepared)
            return false;

        if (base) {
            m_renderer->setDevicePixelRatio(base->devicePixelRatio());
            m_renderer->setDeviceRect(base->deviceRect());
            m_renderer->setViewportRect(base->viewportRect());
            m_renderer->setProjectionMatrix(baseProjectionMatrix);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            m_renderer->setProjectionMatrixWithNativeNDC(
                base->projectionMatrixWithNativeNDC(0));
#else
            m_renderer->setProjectionMatrixWithNativeNDC(
                base->projectionMatrixWithNativeNDC());
#endif
        } else {
            if (dpr.x() <= 0.0f || dpr.y() <= 0.0f)
                return false;

            m_renderer->setDevicePixelRatio(1.0);
            m_renderer->setDeviceRect(QRect(QPoint(0, 0), pixelSize));
            m_renderer->setViewportRect(pixelSize);
            const QRectF rect(
                QPointF(0, 0),
                QSizeF(pixelSize.width() / dpr.x(),
                       pixelSize.height() / dpr.y()));
            // The backdrop is exposed as an ordinary, unmirrored QSGTexture.
            // Match QQuickWindow's projection for a redirected render target
            // so Vulkan's Y-down NDC does not turn the rotated copy upside down.
            m_renderer->setProjectionMatrixToRect(
                rect,
                !m_rhi->isYUpInNDC()
                    ? QSGRenderer::MatrixTransformFlipY
                    : QSGRenderer::MatrixTransformFlag {});
        }

        if (Q_UNLIKELY(!matrix.isIdentity())) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            m_renderer->setProjectionMatrix(
                m_renderer->projectionMatrix(0) * matrix);
            m_renderer->setProjectionMatrixWithNativeNDC(
                m_renderer->projectionMatrixWithNativeNDC(0) * matrix);
#else
            m_renderer->setProjectionMatrix(
                m_renderer->projectionMatrix() * matrix);
            m_renderer->setProjectionMatrixWithNativeNDC(
                m_renderer->projectionMatrixWithNativeNDC() * matrix);
#endif
        }

        auto *oldRoot = m_renderer->rootNode();
        m_renderer->setRootNode(rootNode);
        if (oldRoot != rootNode && WVulkanTrace::enabled()) {
            qCDebug(lcWlRenderBuffer).noquote()
                << QStringLiteral("VKTRACE event=blitter-inline-renderer-root node=%1 role=%2 renderer=%3 old=%4 new=%5")
                       .arg(quintptr(m_blitterNode), 0, 16)
                       .arg(QString::fromLatin1(m_role))
                       .arg(quintptr(m_renderer), 0, 16)
                       .arg(quintptr(oldRoot), 0, 16)
                       .arg(quintptr(rootNode), 0, 16);
        }
        return true;
    }

    bool prepareInline(QRhiRenderTarget *renderTarget,
                       QRhiCommandBuffer *commandBuffer)
    {
        if (!isValid() || !rootNode() || !renderTarget || !commandBuffer
            || m_prepared) {
            return false;
        }

        m_renderer->setRenderTarget(
            {renderTarget, renderTarget->renderPassDescriptor(), commandBuffer});
        auto *defaultContext = static_cast<QSGDefaultRenderContext *>(m_context);
        m_oldDpr = defaultContext->currentDevicePixelRatio();
        m_oldCommandBuffer = defaultContext->currentFrameCommandBuffer();
        m_context->prepareSync(m_renderer->devicePixelRatio(), commandBuffer,
                               m_graphicsConfiguration);

        // These public Qt stages deliberately split resource preparation from
        // draw recording so the caller can begin the compatible render pass in
        // between. They also maintain QSGRenderer's private state themselves.
        m_renderer->prepareSceneInline();
        m_prepared = true;
        return true;
    }

    void renderInline()
    {
        Q_ASSERT(m_renderer && m_prepared);
        m_renderer->renderSceneInline();
        m_context->prepareSync(m_oldDpr, m_oldCommandBuffer,
                               m_graphicsConfiguration);
        m_oldCommandBuffer = nullptr;
        m_prepared = false;
    }

    void detachRoot()
    {
        if (!m_renderer)
            return;
        Q_ASSERT(!m_prepared);
        m_renderer->setRootNode(nullptr);
    }

private:
    QQuickWindow *m_owner = nullptr;
    const void *m_blitterNode = nullptr;
    const char *m_role = nullptr;
    QRhi *m_rhi = nullptr;
    QSGRenderContext *m_context = nullptr;
    QSGRenderer *m_renderer = nullptr;
    QQuickGraphicsConfiguration m_graphicsConfiguration;
    qreal m_oldDpr = 1.0;
    QRhiCommandBuffer *m_oldCommandBuffer = nullptr;
    bool m_prepared = false;
};

static QSizeF mapSize(const QRectF &source, const QMatrix4x4 &matrix)
{
    auto topLeft = matrix.map(source.topLeft());
    auto bottomLeft = matrix.map(source.bottomLeft());
    auto topRight = matrix.map(source.topRight());
    auto bottomRight = matrix.map(source.bottomRight());

    const qreal width1 = std::sqrt(std::pow(topRight.x() - topLeft.x(), 2)
                                   + std::pow(topRight.y() - topLeft.y(), 2));
    const qreal width2 = std::sqrt(std::pow(bottomRight.x() - bottomLeft.x(), 2)
                                   + std::pow(bottomRight.y() - bottomLeft.y(), 2));
    const qreal height1 = std::sqrt(std::pow(bottomLeft.x() - topLeft.x(), 2)
                                    + std::pow(bottomLeft.y() - topLeft.y(), 2));
    const qreal height2 = std::sqrt(std::pow(bottomRight.x() - topRight.x(), 2)
                                    + std::pow(bottomRight.y() - topRight.y(), 2));

    QSizeF size;
    size.setWidth(std::max(width1, width2));
    size.setHeight(std::max(height1, height2));

    return size;
}

inline static void restoreChildNodesTo(QSGNode *node, QSGNode *realParent) {
    Q_ASSERT(realParent->firstChild() == node->firstChild());
    Q_ASSERT(realParent->lastChild() == node->lastChild());

    WQmlHelper::QSGNode_subtreeRenderableCount(node) = 0;
    WQmlHelper::QSGNode_firstChild(node) = nullptr;
    WQmlHelper::QSGNode_lastChild(node) = nullptr;

    node = realParent->firstChild();
    do {
        WQmlHelper::QSGNode_parent(node) = realParent;
        node = node->nextSibling();
    } while (node);
}

inline static void overrideChildNodesTo(QSGNode *node, QSGNode *newParent) {
    Q_ASSERT(newParent->childCount() == 0);
    Q_ASSERT(node->firstChild());
    WQmlHelper::QSGNode_subtreeRenderableCount(newParent) = 0;
    WQmlHelper::QSGNode_firstChild(newParent) = node->firstChild();
    WQmlHelper::QSGNode_lastChild(newParent) = node->lastChild();

    node = newParent->firstChild();
    do {
        WQmlHelper::QSGNode_parent(node) = newParent;
        node->markDirty(QSGNode::DirtyNodeAdded);
        node = node->nextSibling();
    } while (node);
}

class Q_DECL_HIDDEN RhiNode : public WRenderBufferNode {
public:
    RhiNode(QQuickItem *item)
        : WRenderBufferNode(item, new Texture)
    {

    }

    ~RhiNode() {
        destroy();
    }

    StateFlags changedStates() const override {
        return DepthState
               | StencilState
               | ScissorState
               | ColorState
               | BlendState
               | CullState
               | ViewportState
               | RenderTargetState;
    }

    RenderingFlags flags() const override {
        RenderingFlags flags;
        if (!contentNode)
            flags |= BoundedRectRendering;
        const bool isVulkan = m_item && m_item->window()
            && m_item->window()->graphicsApi()
                == QSGRendererInterface::Vulkan;

        // Vulkan backdrop rendering must remain in painter order. Advertising
        // depth awareness lets Qt move opaque nodes above this node ahead of
        // it, so they get copied into the backdrop and are then overpainted by
        // the separately rendered effect content.
        const bool needsPainterOrder = isVulkan;
        if (!needsPainterOrder)
            flags |= DepthAwareRendering;
        if (isVulkan)
            flags |= NoExternalRendering;

        return flags;
    }

    void releaseResources() override {
        destroy();
    }

    QRhiTexture *currentRenderTexture() const {
        auto rt = renderTarget();
        if (rt->resourceType() != QRhiResource::TextureRenderTarget)
            return nullptr;
        auto rhiRT = static_cast<QRhiTextureRenderTarget*>(rt);
        auto rtDesc = rhiRT->description();
        if (rtDesc.colorAttachmentCount() < 1)
            return nullptr;
        return rtDesc.colorAttachmentAt(0)->texture();
    }

    inline WBufferRenderer *maybeBufferRenderer() const {
        const auto currentRenderer = renderWindow()->currentRenderer();
        if (!currentRenderer || !currentRenderer->currentRenderer())
            return nullptr;
        return renderTarget() == currentRenderer->currentRenderer()->renderTarget().rt
                   ? currentRenderer
                   : nullptr;
    }

    void prepare() override {
        contentNode = nullptr;
        vulkanReady = false;
        vulkanResumeTarget = nullptr;
        vulkanCopySourceRect = {};
        vulkanCopyDestination = {};

        if (Q_UNLIKELY(!m_item || !m_item->window())) {
            reset();
            return;
        }

        auto window = renderWindow();
        auto ct = currentRenderTexture();
        if (!ct) {
            reset();
            return;
        }

        auto buffer = WRenderHelper::lookupBuffer(ct);
        if (!buffer) {
            reset();
            return;
        }

        wlr_dmabuf_attributes attribs;
        if (!buffer->get_dmabuf(&attribs)) {
            reset();
            return;
        }

        const auto currentRenderer = maybeBufferRenderer();
        const bool isVulkan = window->rhi()
            && window->rhi()->backend() == QRhi::Vulkan;
        if (isVulkan) {
            if (!currentRenderer) {
                setVulkanUnavailable("not rendering through WBufferRenderer", ct);
                return;
            }
            if (!ct->flags().testFlag(QRhiTexture::UsedAsTransferSource)) {
                setVulkanUnavailable("output modifier does not support transfer-source usage", ct);
                return;
            }
            if (ct->sampleCount() != 1) {
                setVulkanUnavailable("multisampled output copy is unsupported", ct);
                return;
            }

            auto resumeTarget = currentRenderer->currentVulkanBackdropResumeTarget();
            if (!resumeTarget
                || resumeTarget->resourceType() != QRhiResource::TextureRenderTarget
                || renderTarget()->resourceType() != QRhiResource::TextureRenderTarget) {
                setVulkanUnavailable("Vulkan backdrop resume target is unavailable", ct);
                return;
            }

            auto currentTextureTarget = static_cast<QRhiTextureRenderTarget *>(renderTarget());
            auto resumeTextureTarget = static_cast<QRhiTextureRenderTarget *>(resumeTarget);
            const auto currentDesc = currentTextureTarget->description();
            const auto resumeDesc = resumeTextureTarget->description();
            if (currentDesc.colorAttachmentCount() < 1
                || resumeDesc.colorAttachmentCount() < 1
                || currentDesc.colorAttachmentAt(0)->texture() != ct
                || resumeDesc.colorAttachmentAt(0)->texture() != ct
                || !currentDesc.depthTexture()
                || currentDesc.depthTexture() != resumeDesc.depthTexture()
                || currentDesc.depthStencilBuffer()
                || resumeDesc.depthStencilBuffer()
                || !resumeTextureTarget->flags().testFlag(
                    QRhiTextureRenderTarget::PreserveColorContents)
                || !resumeTextureTarget->flags().testFlag(
                    QRhiTextureRenderTarget::PreserveDepthStencilContents)) {
                setVulkanUnavailable("Vulkan backdrop target attachments are incompatible", ct);
                return;
            }
            vulkanResumeTarget = resumeTextureTarget;
        }

        // TODO: Apple viewport to matrix, needs get QSGRenderer
        renderMatrix = currentRenderer
                           ? currentRenderer->currentWorldTransform() * (*this->matrix())
                           : *this->matrix();
        devicePixelRatio = effectiveDevicePixelRatio();
        if (Q_UNLIKELY(!manager || manager->owner() != window)) {
            const auto oldManager = manager;
            manager = RhiTextureManager::resolveByOwner(manager, window);
            sgTexture()->setTexture(nullptr);
            if (oldManager)
                oldManager->release(texture);
            texture.reset();
        }

        Q_ASSERT(ct->rhi() == window->rhi());
        if (Q_UNLIKELY(!rhi || rhi->owner() != window))
            rhi = RhiManager::resolveByOwner(rhi, window);
        Q_ASSERT(rhi);

        hasRotation = renderMatrix.flags().testAnyFlags(
            QMatrix4x4::Rotation2D | QMatrix4x4::Rotation);
        QSize pixelSize;

        if (hasRotation) {
            const QSizeF size = mapSize(m_rect, renderMatrix) * devicePixelRatio;
            if (size.isEmpty()) {
                reset();
                return;
            }

            pixelSize = size.toSize();

            if (!renderData) {
                renderData.reset(new RenderData);

                renderData->imageNode = window->createImageNode();
                renderData->imageNode->setFlag(QSGNode::OwnedByParent);
                renderData->imageNode->setOwnsTexture(false);
                renderData->texture.setOwnsTexture(false);
                renderData->imageNode->setTexture(&renderData->texture);
                renderData->rootNode.appendChildNode(renderData->imageNode);
            }
        } else {
            vulkanRotationRenderer.reset();
            renderData.reset();

            QSizeF size = renderMatrix.mapRect(m_rect).size() * devicePixelRatio;
            if (!size.isValid()) {
                reset();
                return;
            }

            pixelSize = size.toSize();
        }

        if (pixelSize.isEmpty()) {
            reset();
            return;
        }

        if (isVulkan) {
            const QRect renderTargetRect(QPoint(0, 0), ct->pixelSize());
            if (hasRotation) {
                // Keep the same source coordinate domain as the GLES2 path.
                // A projective rotation and a fractional DPR do not commute,
                // so a DPR-scaled mapped bounding box can omit pixels that the
                // inverse transform still samples.
                vulkanCopySourceRect = renderTargetRect;
                vulkanCopyDestination = QPoint(0, 0);
            } else {
                const QPoint sourcePos =
                    (renderMatrix.map(m_rect.topLeft()) * devicePixelRatio).toPoint();
                vulkanCopySourceRect = QRect(sourcePos, pixelSize)
                    .intersected(renderTargetRect);
                vulkanCopyDestination = vulkanCopySourceRect.topLeft() - sourcePos;
            }

            if (vulkanCopySourceRect.isEmpty()) {
                reset();
                return;
            }
        }

        {
            auto format = attribs.format;
            auto modifier = attribs.modifier;
            QRhiTexture::Flags textureFlags = isVulkan
                ? ct->flags() & QRhiTexture::sRGB
                : QRhiTexture::Flags {};
            texture = manager->resolve(texture, ct->format(), std::move(format),
                                       std::move(modifier), pixelSize,
                                       std::move(textureFlags));
        }
        if (Q_UNLIKELY(texture.expired())) {
            reset();
            return;
        }
        auto texture = this->texture.lock();
        Q_ASSERT(texture->data);

        if (renderData) {
            if (!renderData->rt || sgTexture()->rhiTexture() != texture->data->rhiTexture) {
                QRhiTextureRenderTargetDescription rtDesc(texture->data->rhiTexture);
                const auto flags = isVulkan
                    ? QRhiTextureRenderTarget::Flags {}
                    : QRhiTextureRenderTarget::PreserveColorContents
                        | QRhiTextureRenderTarget::PreserveDepthStencilContents;
                auto newRT = rhi->rhi()->newTextureRenderTarget(rtDesc, flags);
                newRT->setRenderPassDescriptor(newRT->newCompatibleRenderPassDescriptor());
                if (!newRT->create()) {
                    delete newRT;
                    if (isVulkan)
                        setVulkanUnavailable("failed to create rotated backdrop render target", ct);
                    return;
                }

                renderData->rt.reset(newRT);
            }

            if (isVulkan
                && (!renderData->scratchTexture
                    || renderData->scratchTexture->format() != ct->format()
                    || renderData->scratchTexture->pixelSize() != vulkanCopySourceRect.size()
                    || (renderData->scratchTexture->flags() & QRhiTexture::sRGB)
                        != (ct->flags() & QRhiTexture::sRGB))) {
                const QRhiTexture::Flags scratchFlags = ct->flags() & QRhiTexture::sRGB;
                std::unique_ptr<QRhiTexture> scratchTexture(
                    rhi->rhi()->newTexture(ct->format(), vulkanCopySourceRect.size(),
                                           1, scratchFlags));
                scratchTexture->setName(
                    QByteArrayLiteral("WaylibRenderBufferNodeScratchTexture"));
                if (!scratchTexture->create()) {
                    setVulkanUnavailable("failed to create rotated backdrop scratch texture", ct);
                    return;
                }
                renderData->scratchTexture = std::move(scratchTexture);
            }
        }

        if (m_content) {
            auto rootNode = WQmlHelper::getRootNode(m_content);
            if (rootNode && rootNode->firstChild()) {
                contentNode = rootNode;
            }
        }

        if (isVulkan) {
            const auto ensureInlineRenderer = [&] (
                std::unique_ptr<VulkanInlineRenderer> &inlineRenderer,
                const char *role) {
                if (inlineRenderer && inlineRenderer->owner() != window)
                    inlineRenderer.reset();
                if (!inlineRenderer) {
                    inlineRenderer = std::make_unique<VulkanInlineRenderer>(
                        window, this, role);
                }
                return inlineRenderer->isValid();
            };

            if (hasRotation
                && !ensureInlineRenderer(vulkanRotationRenderer, "rotation")) {
                setVulkanUnavailable(
                    "failed to create the Vulkan rotation inline renderer", ct);
                return;
            }

            if (contentNode) {
                if (!ensureInlineRenderer(vulkanContentRenderer, "content")) {
                    setVulkanUnavailable(
                        "failed to create the Vulkan content inline renderer", ct);
                    return;
                }
            } else {
                vulkanContentRenderer.reset();
            }
        }

        vulkanReady = isVulkan;
        if (isVulkan && WVulkanTrace::enabled()) {
            const bool outerDepthEnabled = currentRenderer
                && currentRenderer->currentBatchRenderer()
                && rendererUseDepthBuffer(
                    currentRenderer->currentBatchRenderer());
            qCDebug(lcWlRenderBuffer).noquote()
                << QStringLiteral("VKTRACE event=blitter-prepare node=%1 item=%2 current=%3 resume=%4 color=%5 depth=%6 copyX=%7 copyY=%8 copyW=%9 copyH=%10 dstX=%11 dstY=%12 rotated=%13 content=%14 ordering=%15 depthAware=%16 outerDepth=%17")
                       .arg(quintptr(this), 0, 16)
                       .arg(quintptr(m_item.data()), 0, 16)
                       .arg(quintptr(renderTarget()), 0, 16)
                       .arg(quintptr(vulkanResumeTarget), 0, 16)
                       .arg(quintptr(ct), 0, 16)
                       .arg(quintptr(static_cast<QRhiTextureRenderTarget *>(renderTarget())
                                         ->description().depthTexture()), 0, 16)
                       .arg(vulkanCopySourceRect.x())
                       .arg(vulkanCopySourceRect.y())
                       .arg(vulkanCopySourceRect.width())
                       .arg(vulkanCopySourceRect.height())
                       .arg(vulkanCopyDestination.x())
                       .arg(vulkanCopyDestination.y())
                       .arg(hasRotation)
                       .arg(bool(contentNode))
                       .arg(QStringLiteral("painter"))
                       .arg(false)
                       .arg(outerDepthEnabled);
            if (hasRotation) {
                qCDebug(lcWlRenderBuffer).noquote()
                    << QStringLiteral("VKTRACE event=blitter-rotation-geometry node=%1 copyX=%2 copyY=%3 copyW=%4 copyH=%5 targetW=%6 targetH=%7 backdropW=%8 backdropH=%9 dpr=%10")
                           .arg(quintptr(this), 0, 16)
                           .arg(vulkanCopySourceRect.x())
                           .arg(vulkanCopySourceRect.y())
                           .arg(vulkanCopySourceRect.width())
                           .arg(vulkanCopySourceRect.height())
                           .arg(ct->pixelSize().width())
                           .arg(ct->pixelSize().height())
                           .arg(pixelSize.width())
                           .arg(pixelSize.height())
                           .arg(devicePixelRatio, 0, 'f', 3);
            }
        }
    }

    void render([[maybe_unused]] const RenderState *state) override {
        auto texture = this->texture.lock();
        if (Q_UNLIKELY(!texture))
            return;
        auto rhiTexture = texture->data->rhiTexture;

        auto ct = currentRenderTexture();
        Q_ASSERT(ct);

        if (renderWindow()->rhi()
            && renderWindow()->rhi()->backend() == QRhi::Vulkan) {
            renderVulkan(ct, rhiTexture);
            return;
        }

        if (renderData) {
            renderData->texture.setTexture(ct);
            renderData->texture.setTextureSize(ct->pixelSize());

            const QPointF sourcePos = renderMatrix.map(m_rect.topLeft());
            renderData->imageNode->setRect(QRectF(-(devicePixelRatio - 1) * sourcePos, ct->pixelSize()));

            rhi->sync(rhiTexture->pixelSize(), &renderData->rootNode, renderMatrix.inverted(), {}, nullptr,
                      {rhiTexture->pixelSize().width() / float(m_rect.width() * devicePixelRatio),
                       rhiTexture->pixelSize().height() / float(m_rect.height() * devicePixelRatio)});
            rhi->render(renderData->rt.get());
        } else {
            const QPoint sourcePos = (renderMatrix.map(m_rect.topLeft()) * devicePixelRatio).toPoint();
            const QRect sourceTextureRect(sourcePos, rhiTexture->pixelSize());
            const QRect renderTargetRect(QPoint(0, 0), ct->pixelSize());
            const QRect copySourceRect = sourceTextureRect.intersected(renderTargetRect);
            if (copySourceRect.isEmpty())
                return;

            auto rhi = this->rhi->rhi();
            auto rub = rhi->nextResourceUpdateBatch();
            QRhiTextureCopyDescription desc;
            desc.setPixelSize(copySourceRect.size());
            desc.setSourceTopLeft(copySourceRect.topLeft());
            desc.setDestinationTopLeft(copySourceRect.topLeft() - sourcePos);
            rub->copyTexture(rhiTexture, ct, desc);

            QRhiCommandBuffer *cb = nullptr;
            if (rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
                return;
            Q_ASSERT(cb);

            // TODO: needs vkCmdPipelineBarrier?
            cb->resourceUpdate(rub);
            rhi->endOffscreenFrame();
        }

        if (sgTexture()->rhiTexture() != rhiTexture)
            sgTexture()->setTexture(rhiTexture);
        doNotifyTextureChanged();

        if (contentNode) {
            Q_ASSERT(renderTarget()->resourceType() == QRhiResource::TextureRenderTarget);
            auto textureRT = static_cast<QRhiTextureRenderTarget*>(renderTarget());
            auto saveFlags = textureRT->flags();
            textureRT->setFlags(QRhiTextureRenderTarget::PreserveColorContents
                                | QRhiTextureRenderTarget::PreserveDepthStencilContents);
            auto currentRenderer = maybeBufferRenderer();
            // If the source renderer enable depth test, we should enable depth test also,
            // to ensure the contentNode's z order on RenderMode2DNoDepthBuffer render mode.
            const bool forceDepthTest = currentRenderer && currentRenderer->currentBatchRenderer()
                                   && rendererUseDepthBuffer(currentRenderer->currentBatchRenderer());

            if (clipList() || inheritedOpacity() < 1.0) {
                if (!node)
                    node.reset(new Node);

                node->opacityNode.setOpacity(inheritedOpacity());
                node->transformNode.setMatrix(*this->matrix());

                QSGNode *childContainer = &node->opacityNode;
                if (clipList()) {
                    if (!node->clipNode) {
                        node->clipNode = new QQuickDefaultClipNode(QRectF(0, 0, 65535, 65535));
                        node->clipNode->setFlag(QSGNode::OwnedByParent, false);
                        node->clipNode->setClipRect(node->clipNode->rect());
                        node->clipNode->update();

                        node->rootNode.reparentChildNodesTo(node->clipNode);
                        node->rootNode.appendChildNode(node->clipNode);
                    }
                } else {
                    if (node->clipNode)
                        node->clipNode->reparentChildNodesTo(&node->rootNode);

                    delete node->clipNode;
                    node->clipNode = nullptr;
                }

                overrideChildNodesTo(contentNode, childContainer);

                rhi->sync(ct->pixelSize(), &node->rootNode, {}, *projectionMatrix(),
                          currentRenderer ? currentRenderer->currentRenderer() : nullptr);
                qreal oldDPR;
                QRhiCommandBuffer *oldCB;
                if (rhi->preprocess(textureRT, oldDPR, oldCB)) {
                    if (node->clipNode) {
                        if (!node->clipNode->clipList()) {
                            node->clipNode->setRendererClipList(clipList());
                        } else {
                            auto lastClipNode = node->clipNode->clipList();
                            while (auto cliplist = lastClipNode->clipList())
                                lastClipNode = cliplist;
                            Q_ASSERT(lastClipNode->clipList());
                            const_cast<QSGClipNode*>(lastClipNode)->setRendererClipList(clipList());
                        }
                    }

                    rhi->render(oldDPR, oldCB, forceDepthTest);
                }

                restoreChildNodesTo(childContainer, contentNode);
            } else {
                node.reset();
                rhi->sync(ct->pixelSize(), contentNode, *this->matrix(), *projectionMatrix(),
                          currentRenderer ? currentRenderer->currentRenderer() : nullptr);
                rhi->render(textureRT, forceDepthTest);
            }

            textureRT->setFlags(saveFlags);
        }
    }

private:
    void setVulkanUnavailable(const char *reason, QRhiTexture *renderTexture) {
        vulkanReady = false;
        vulkanResumeTarget = nullptr;
        const bool hadTexture = sgTexture()->rhiTexture();
        reset(false);
        if (hadTexture)
            doNotifyTextureChanged();
        if (vulkanUnavailableLogged)
            return;

        qCWarning(lcWlRenderBuffer) << "Disabled RenderBufferBlitter for Vulkan render target"
                                    << "reason" << reason
                                    << "item" << m_item
                                    << "itemSize" << m_size
                                    << "renderTargetSize"
                                    << (renderTexture ? renderTexture->pixelSize() : QSize());
        vulkanUnavailableLogged = true;
    }

    void renderVulkan(QRhiTexture *currentTexture, QRhiTexture *backdropTexture) {
        if (!vulkanReady || !vulkanResumeTarget || !rhi)
            return;

        const quint64 passSequence = ++vulkanPassSequence;

        auto cb = commandBuffer();
        if (!cb) {
            setVulkanUnavailable("missing current QRhi command buffer", currentTexture);
            return;
        }
        if (hasRotation
            && (!renderData || !renderData->rt || !renderData->scratchTexture)) {
            setVulkanUnavailable("rotated backdrop resources are incomplete", currentTexture);
            return;
        }

        auto rub = rhi->rhi()->nextResourceUpdateBatch();
        QRhiTextureCopyDescription copyDescription;
        copyDescription.setPixelSize(vulkanCopySourceRect.size());
        copyDescription.setSourceTopLeft(vulkanCopySourceRect.topLeft());
        copyDescription.setDestinationTopLeft(vulkanCopyDestination);
        rub->copyTexture(hasRotation ? renderData->scratchTexture.get() : backdropTexture,
                         currentTexture,
                         copyDescription);

        if (WVulkanTrace::enabled()) {
            qCDebug(lcWlRenderBuffer).noquote()
                << QStringLiteral("VKTRACE event=blitter-pass node=%1 sequence=%2 phase=end-main-and-copy commandBuffer=%3 current=%4 backdrop=%5 resume=%6")
                       .arg(quintptr(this), 0, 16)
                       .arg(passSequence)
                       .arg(quintptr(cb), 0, 16)
                       .arg(quintptr(currentTexture), 0, 16)
                       .arg(quintptr(backdropTexture), 0, 16)
                       .arg(quintptr(vulkanResumeTarget), 0, 16);
        }

        // Vulkan cannot copy an image while it is an active color attachment.
        // Split the outer pass, keep every operation on Qt's command buffer,
        // then resume on a render-pass-compatible target that loads both the
        // shared color and depth-stencil attachments.
        cb->endPass(rub);
        bool outerPassActive = false;
        const auto passFlags = QRhiCommandBuffer::ExternalContent
            | QRhiCommandBuffer::DoNotTrackResourcesForCompute;
        const auto resumeOuterPass = [&] {
            if (outerPassActive)
                return;
            cb->beginPass(vulkanResumeTarget,
                          Qt::transparent,
                          QRhiDepthStencilClearValue(1.0f, 0),
                          nullptr,
                          passFlags);
            outerPassActive = true;
            if (WVulkanTrace::enabled()) {
                qCDebug(lcWlRenderBuffer).noquote()
                    << QStringLiteral("VKTRACE event=blitter-pass node=%1 sequence=%2 phase=resume-main commandBuffer=%3 resume=%4")
                           .arg(quintptr(this), 0, 16)
                           .arg(passSequence)
                           .arg(quintptr(cb), 0, 16)
                           .arg(quintptr(vulkanResumeTarget), 0, 16);
            }
        };
        const auto passGuard = qScopeGuard(resumeOuterPass);

        auto *depthStencilTexture = static_cast<QRhiTextureRenderTarget *>(
                                        renderTarget())
                                        ->description()
                                        .depthTexture();
        if (!recordVulkanDepthStencilPassBarrier(
                rhi->rhi(), cb, depthStencilTexture)) {
            // Restore the outer pass before disabling this node. The caller
            // still owns that pass and expects it to remain active.
            resumeOuterPass();
            setVulkanUnavailable(
                "failed to record the depth-stencil pass barrier",
                currentTexture);
            return;
        }
        if (WVulkanTrace::enabled()) {
            qCDebug(lcWlRenderBuffer).noquote()
                << QStringLiteral("VKTRACE event=blitter-pass node=%1 sequence=%2 phase=depth-stencil-barrier depth=%3")
                       .arg(quintptr(this), 0, 16)
                       .arg(passSequence)
                       .arg(quintptr(depthStencilTexture), 0, 16);
        }

        if (hasRotation) {
            if (!vulkanRotationRenderer) {
                setVulkanUnavailable(
                    "missing Vulkan rotation inline renderer", currentTexture);
                return;
            }

            auto scratchTexture = renderData->scratchTexture.get();
            renderData->texture.setTexture(scratchTexture);
            renderData->texture.setTextureSize(scratchTexture->pixelSize());

            const QPointF sourcePos = renderMatrix.map(m_rect.topLeft());
            const QRectF imageRect(
                -(devicePixelRatio - 1) * sourcePos,
                scratchTexture->pixelSize());
            const QRectF imageSourceRect(
                QPointF(0, 0), scratchTexture->pixelSize());

            // The full-size scratch texture deliberately reproduces the GLES2
            // image geometry. This keeps every texel reachable by the inverse
            // projective transform, including at fractional DPR.
            renderData->imageNode->setSourceRect(imageSourceRect);
            renderData->imageNode->setRect(imageRect);

            if (WVulkanTrace::enabled()) {
                qCDebug(lcWlRenderBuffer).noquote()
                    << QStringLiteral("VKTRACE event=blitter-rotation-draw node=%1 sequence=%2 rectX=%3 rectY=%4 rectW=%5 rectH=%6 sourceX=%7 sourceY=%8 sourceW=%9 sourceH=%10 scratchW=%11 scratchH=%12")
                           .arg(quintptr(this), 0, 16)
                           .arg(passSequence)
                           .arg(imageRect.x(), 0, 'f', 3)
                           .arg(imageRect.y(), 0, 'f', 3)
                           .arg(imageRect.width(), 0, 'f', 3)
                           .arg(imageRect.height(), 0, 'f', 3)
                           .arg(imageSourceRect.x(), 0, 'f', 3)
                           .arg(imageSourceRect.y(), 0, 'f', 3)
                           .arg(imageSourceRect.width(), 0, 'f', 3)
                           .arg(imageSourceRect.height(), 0, 'f', 3)
                           .arg(scratchTexture->pixelSize().width())
                           .arg(scratchTexture->pixelSize().height());
            }

            if (!vulkanRotationRenderer->sync(
                    backdropTexture->pixelSize(),
                    &renderData->rootNode,
                    renderMatrix.inverted(),
                    {},
                    nullptr,
                    {backdropTexture->pixelSize().width()
                         / float(m_rect.width() * devicePixelRatio),
                     backdropTexture->pixelSize().height()
                         / float(m_rect.height() * devicePixelRatio)})) {
                setVulkanUnavailable(
                    "failed to synchronize the Vulkan rotation inline renderer",
                    currentTexture);
                return;
            }
            if (!vulkanRotationRenderer->prepareInline(renderData->rt.get(), cb)) {
                setVulkanUnavailable(
                    "failed to prepare the Vulkan rotation inline renderer",
                    currentTexture);
                return;
            }
            cb->beginPass(renderData->rt.get(),
                          Qt::transparent,
                          QRhiDepthStencilClearValue(1.0f, 0),
                          nullptr,
                          passFlags);
            vulkanRotationRenderer->renderInline();
            cb->endPass();
            if (WVulkanTrace::enabled()) {
                qCDebug(lcWlRenderBuffer).noquote()
                    << QStringLiteral("VKTRACE event=blitter-pass node=%1 sequence=%2 phase=rotation-complete renderer=%3 root=%4")
                           .arg(quintptr(this), 0, 16)
                           .arg(passSequence)
                           .arg(quintptr(vulkanRotationRenderer->renderer()), 0, 16)
                           .arg(quintptr(vulkanRotationRenderer->rootNode()), 0, 16);
            }
        }

        if (sgTexture()->rhiTexture() != backdropTexture)
            sgTexture()->setTexture(backdropTexture);
        doNotifyTextureChanged();

        if (!contentNode)
            return;

        if (!vulkanContentRenderer) {
            setVulkanUnavailable(
                "missing Vulkan content inline renderer", currentTexture);
            return;
        }

        auto currentRenderer = maybeBufferRenderer();
        if (!currentRenderer) {
            reset();
            return;
        }

        QSGNode *childContainer = nullptr;
        if (clipList() || inheritedOpacity() < 1.0) {
            if (!node)
                node.reset(new Node);

            node->opacityNode.setOpacity(inheritedOpacity());
            node->transformNode.setMatrix(*this->matrix());
            childContainer = &node->opacityNode;
            if (clipList()) {
                if (!node->clipNode) {
                    node->clipNode = new QQuickDefaultClipNode(
                        QRectF(0, 0, 65535, 65535));
                    node->clipNode->setFlag(QSGNode::OwnedByParent, false);
                    node->clipNode->setClipRect(node->clipNode->rect());
                    node->clipNode->update();

                    node->rootNode.reparentChildNodesTo(node->clipNode);
                    node->rootNode.appendChildNode(node->clipNode);
                }
            } else {
                if (node->clipNode)
                    node->clipNode->reparentChildNodesTo(&node->rootNode);

                delete node->clipNode;
                node->clipNode = nullptr;
            }

            overrideChildNodesTo(contentNode, childContainer);
        }

        const auto childGuard = qScopeGuard([&] {
            if (childContainer) {
                // The temporary wrapper borrows the content subtree. Detach
                // the dedicated renderer while the borrowed nodes are still
                // below its root, then restore their real parent.
                if (vulkanContentRenderer)
                    vulkanContentRenderer->detachRoot();
                restoreChildNodesTo(childContainer, contentNode);
            }
        });

        QSGRootNode *contentRenderRoot = nullptr;
        QMatrix4x4 contentMatrix;
        if (childContainer) {
            contentRenderRoot = &node->rootNode;
        } else {
            node.reset();
            contentRenderRoot = contentNode;
            contentMatrix = *this->matrix();
        }

        if (node && node->clipNode) {
            if (!node->clipNode->clipList()) {
                node->clipNode->setRendererClipList(clipList());
            } else {
                auto lastClipNode = node->clipNode->clipList();
                while (auto cliplist = lastClipNode->clipList())
                    lastClipNode = cliplist;
                Q_ASSERT(!lastClipNode->clipList());
                const_cast<QSGClipNode *>(lastClipNode)->setRendererClipList(clipList());
            }
        }

        if (!vulkanContentRenderer->sync(
                currentTexture->pixelSize(),
                contentRenderRoot,
                contentMatrix,
                *projectionMatrix(),
                currentRenderer->currentRenderer())) {
            setVulkanUnavailable(
                "failed to synchronize the Vulkan content inline renderer",
                currentTexture);
            return;
        }
        if (!vulkanContentRenderer->prepareInline(vulkanResumeTarget, cb)) {
            setVulkanUnavailable(
                "failed to prepare the Vulkan content inline renderer",
                currentTexture);
            return;
        }

        resumeOuterPass();
        vulkanContentRenderer->renderInline();
        if (WVulkanTrace::enabled()) {
            qCDebug(lcWlRenderBuffer).noquote()
                << QStringLiteral("VKTRACE event=blitter-pass node=%1 sequence=%2 phase=content-complete renderer=%3 root=%4")
                       .arg(quintptr(this), 0, 16)
                       .arg(passSequence)
                       .arg(quintptr(vulkanContentRenderer->renderer()), 0, 16)
                       .arg(quintptr(vulkanContentRenderer->rootNode()), 0, 16);
        }
    }

    void reset(bool notifyTexture = true) {
        if (renderData)
            renderData->rt.reset();

        if (!sgTexture()->rhiTexture() && notifyTexture)
            doNotifyTextureChanged();
        sgTexture()->setTexture(nullptr);
        if (!texture.expired() && manager)
            manager->release(texture.lock());
        texture.reset();
    }

    void destroy() {
        reset(false);
        // Inline renderers must detach from their roots before the temporary
        // root containers below are destroyed.
        vulkanContentRenderer.reset();
        vulkanRotationRenderer.reset();
        renderData.reset();
        node.reset();
        manager = nullptr;
        texture.reset();
    }

    DataManagerPointer<RhiTextureManager> manager;
    std::weak_ptr<RhiTextureManager::Data> texture;
    DataManagerPointer<RhiManager> rhi;
    QMatrix4x4 renderMatrix;
    qreal devicePixelRatio;
    bool hasRotation = false;
    bool vulkanReady = false;
    bool vulkanUnavailableLogged = false;
    quint64 vulkanPassSequence = 0;
    QRhiTextureRenderTarget *vulkanResumeTarget = nullptr;
    QRect vulkanCopySourceRect;
    QPoint vulkanCopyDestination;
    std::unique_ptr<VulkanInlineRenderer> vulkanContentRenderer;
    std::unique_ptr<VulkanInlineRenderer> vulkanRotationRenderer;

    struct Node {
        Node() {
            transformNode.setFlag(QSGNode::OwnedByParent, false);
            opacityNode.setFlag(QSGNode::OwnedByParent, false);
            rootNode.setFlag(QSGNode::OwnedByParent, false);
            transformNode.appendChildNode(&opacityNode);
            rootNode.appendChildNode(&transformNode);
        }

        QSGRootNode rootNode;
        QSGTransformNode transformNode;
        QSGOpacityNode opacityNode;
        QQuickDefaultClipNode *clipNode = nullptr;
    };

    std::unique_ptr<Node> node;
    QSGRootNode *contentNode = nullptr;

    struct RenderData {
        struct QRhiTextureRenderTargetDeleter {
            inline void operator()(QRhiTextureRenderTarget *pointer) const {
                if (pointer) {
                    delete pointer->renderPassDescriptor();
                    pointer->setRenderPassDescriptor(nullptr);
                    pointer->deleteLater();
                }
            }
        };

        std::unique_ptr<QRhiTextureRenderTarget, QRhiTextureRenderTargetDeleter> rt;
        std::unique_ptr<QRhiTexture> scratchTexture;
        QSGRootNode rootNode;
        QSGImageNode *imageNode;
        QSGPlainTexture texture;
    };

    std::unique_ptr<RenderData> renderData;

    struct Texture : public QSGDynamicTexture {
        void setTexture(QRhiTexture *texture) {
            if (texture)
                m_textureSize = texture->pixelSize();
            m_texture = texture;
        }

        bool updateTexture() override {
            return true;
        }

        qint64 comparisonKey() const override {
            if (m_texture)
                return qint64(m_texture);

            return qint64(this);
        }

        QRhiTexture *rhiTexture() const override {
            return m_texture;
        }

        QSize textureSize() const override {
            return m_textureSize;
        }

        bool hasAlphaChannel() const override {
            return true;
        }

        bool hasMipmaps() const override {
            return mipmapFiltering() != QSGTexture::None;
        }

        QRhiTexture *m_texture = nullptr;
        QSize m_textureSize;
    };

    inline Texture *sgTexture() const {
        return static_cast<Texture*>(m_texture.get());
    }
};

QSGTexture *WRenderBufferNode::texture() const
{
    return m_texture.data();
}

WRenderBufferNode *WRenderBufferNode::createRhiNode(QQuickItem *item)
{
    auto node = new RhiNode(item);
    return node;
}

class Q_DECL_HIDDEN QImageManager : public DataManager<QImageManager, QImage, QImage::Format, const QSize&>
{
    Q_OBJECT

    friend class DataManager;

    QImageManager(QQuickWindow *owner)
        : DataManager<QImageManager, QImage, QImage::Format, const QSize&>(owner) {
        Q_ASSERT(owner->findChildren<QImageManager*>(Qt::FindDirectChildrenOnly).size() == 1);
    }

    static bool check(QImage *image, QImage::Format format, const QSize &size) {
        return image->format() == format && image->size() == size;
    }

    QImage *create(QImage::Format format, const QSize &size) {
        return new QImage(size, format);
    }

    static void destroy(QImage *image) {
        delete image;
    }
};

class Q_DECL_HIDDEN SoftwareNode : public WRenderBufferNode {
public:
    SoftwareNode(QQuickItem *item)
        : WRenderBufferNode(item, new QSGPlainTexture)
    {
        texture()->setOwnsTexture(false);
        // Ensuse always render on software renderer
        texture()->setHasAlphaChannel(true);
    }

    ~SoftwareNode() {
        destroy();
    }

    void releaseResources() override {
        destroy();
    }

    QImage toImage() const override
    {
        return image.expired() ? QImage() : *image.lock()->data;
    }

    void render([[maybe_unused]] const RenderState *state) override {
        auto window = renderWindow();
        if (!window)
            return;
        QSGRendererInterface *rif = window->rendererInterface();
        QPainter *p = static_cast<QPainter *>(rif->getResource(window,
                                                               QSGRendererInterface::PainterResource));
        Q_ASSERT(p);

        // const auto currentRenderer = window->currentRenderer();
        // const auto sgRenderer = currentRenderer ? currentRenderer->currentRenderer() : nullptr;
        const auto matrix = /*(sgRenderer && sgRenderer->renderTarget().paintDevice == p->device())
            ? currentRenderer->currentWorldTransform() * (*this->matrix()) :*/ *this->matrix();
        const auto oldManager = manager;
        manager = QImageManager::resolveByOwner(manager, window);

        if (oldManager != manager) {
            texture()->setTexture(nullptr);
            if (oldManager)
                oldManager->release(image);
            image.reset();
        }

        const bool hasRotation = matrix.flags().testAnyFlags(QMatrix4x4::Rotation2D | QMatrix4x4::Rotation);
        QSizeF size;

        if (hasRotation) {
            size = mapSize(m_rect, matrix);
        } else {
            size = matrix.mapRect(m_rect).size();
        }

        if (size.isEmpty()) {
            reset();
            return;
        }

        const qreal dpr = effectiveDevicePixelRatio();
        size *= dpr;
        const QSize pixelSize = size.toSize();
        const auto device = p->device();

        QImage sourceImage;
        QPixmap sourcePixmap;

        if (Q_LIKELY(device->devType() == QInternal::CustomRaster)) {
            auto rt = static_cast<WImageRenderTarget*>(device);
            sourceImage = rt->operator const QImage &();
        } else if (device->devType() == QInternal::Image) {
            sourceImage = *static_cast<QImage*>(device);
        } else if (device->devType() == QInternal::Pixmap) {
            sourcePixmap = *static_cast<QPixmap*>(device);
        } else {
            return;
        }

        if (Q_UNLIKELY(sourceImage.isNull())) {
            image = manager->resolve(image, QImage::Format_RGB30, pixelSize);
        } else {
            image = manager->resolve(image, sourceImage.format(), pixelSize);
        }

        auto image = this->image.lock();
        painter.begin(image->data);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        auto transform = matrix.toTransform().inverted();
        QTransform resetPos;
        resetPos.translate((dpr - 1) * transform.dx(),
                           (dpr - 1) * transform.dy());
        painter.setTransform(transform * resetPos);

        // TODO: copy damage area from the previous frame
        if (Q_UNLIKELY(sourceImage.isNull())) {
            painter.drawPixmap(sourcePixmap.rect(), sourcePixmap, sourcePixmap.rect());
        } else {
            painter.drawImage(sourceImage.rect(), sourceImage, sourceImage.rect());
        }

        painter.end();

        texture()->setImage(*image->data);
        // Ensuse always render on software renderer
        texture()->setHasAlphaChannel(true);
        doNotifyTextureChanged();
    }

private:
    inline QSGPlainTexture *texture() const {
        return static_cast<QSGPlainTexture*>(m_texture.get());
    }

    void reset(bool notifyTexture = true) {
        if (!texture()->image().isNull() && notifyTexture)
            doNotifyTextureChanged();
        texture()->setTexture(nullptr);
        if (manager)
            manager->release(image);
        image.reset();
    }

    void destroy() {
        reset(false);
        manager = nullptr;
    }

    friend class WRenderBufferNode;
    DataManagerPointer<QImageManager> manager;
    std::weak_ptr<QImageManager::Data> image;
    QPainter painter;
};

WRenderBufferNode *WRenderBufferNode::createSoftwareNode(QQuickItem *item)
{
    auto node = new SoftwareNode(item);
    return node;
}

QRectF WRenderBufferNode::rect() const
{
    return QRectF(0, 0, m_item->width(), m_item->height());
}

WRenderBufferNode::RenderingFlags WRenderBufferNode::flags() const
{
    return BoundedRectRendering | DepthAwareRendering;
}

void WRenderBufferNode::resize(const QSizeF &size)
{
    if (m_size == size)
        return;
    m_size = size;
    m_rect = QRectF(QPointF(0, 0), m_size);
}

void WRenderBufferNode::setContentItem(QQuickItem *item)
{
    if (m_content == item)
        return;
    m_content = item;
    markDirty(DirtyMaterial);
}

void WRenderBufferNode::setTextureChangedCallback(TextureChangedNotifer callback, void *data)
{
    m_renderCallback = callback;
    m_callbackData = data;
}

WOutputRenderWindow *WRenderBufferNode::renderWindow() const
{
    Q_ASSERT(m_item);
    auto rw = qobject_cast<WOutputRenderWindow*>(m_item->window());
    Q_ASSERT(rw);
    return rw;
}

qreal WRenderBufferNode::effectiveDevicePixelRatio() const
{
    auto window = renderWindow();
    auto renderer = window->currentRenderer();
    if (!renderer)
        return window->effectiveDevicePixelRatio();
    auto sgRenderer = renderer->currentRenderer();
    if (!sgRenderer || sgRenderer->renderTarget().rt != renderTarget())
        return window->effectiveDevicePixelRatio();

    return renderer->currentDevicePixelRatio();
}

WRenderBufferNode::WRenderBufferNode(QQuickItem *item, QSGTexture *texture)
    : m_item(item)
    , m_texture(texture)
{

}

WAYLIB_SERVER_END_NAMESPACE

#include "wrenderbuffernode.moc"
