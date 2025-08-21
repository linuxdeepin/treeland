// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QDebug>
#define protected public
#define private public
#include <private/qsgrenderer_p.h>
#include <private/qsgbatchrenderer_p.h>
#undef protected
#undef private

#include "wrenderbuffernode_p.h"
#include "wbufferrenderer_p.h"
#include "wqmlhelper_p.h"
#include "platformplugin/types.h"

#include <QQuickItem>
#include <QRunnable>
#include <QSGImageNode>
#include <private/qquickitem_p.h>
#include <private/qsgplaintexture_p.h>
#include <private/qrhi_p.h>
#include <private/qrhivulkan_p.h>
#include <private/qsgrenderer_p.h>
#include <private/qsgdefaultrendercontext_p.h>
#include <private/qsgrhisupport_p.h>
#include <private/qquickrendercontrol_p.h>
#include <qobjectdefs.h>

#include <algorithm>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN DataManagerBase : public QObject
{
public:
    mutable QAtomicInt ref;

    explicit DataManagerBase(QQuickWindow *owner)
        : QObject(owner) {}
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
        deref();
        pointer = other.pointer;
        ref();
        return *this;
    }

    DataManagerPointer &operator=(DataManagerPointer<T> &&other) noexcept
    {
        pointer = std::exchange(other.pointer, nullptr);
        return *this;
    }

    ~DataManagerPointer() {
        deref();
    }

    inline DataManagerPointer<T> &operator=(T* p) {
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

    static DataManagerPointer<Derive> resolve(const DataManagerPointer<Derive> &other, QQuickWindow *owner) {
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

        auto newData = std::shared_ptr<Data>(new Data());
        if ((newData->data = get()->create(std::forward<DataKeys>(keys)...))) {
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

            QList<std::shared_ptr<Data>> tmp;
            std::swap(manager->dataList, tmp);
            manager->dataList.reserve(tmp.size());

            for (const auto &data : std::as_const(tmp)) {
                if (data->released > 2) {
                    manager->get()->destroy(data->data);
                } else {
                    manager->dataList << data;

                    if (data->released > 0)
                        ++data->released;
                }
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

    DataManager(QQuickWindow *owner)
        : DataManagerBase(owner) {
        Q_ASSERT(owner->findChildren<Derive*>(Qt::FindDirectChildrenOnly).size() == 0);
    }

    using QObject::deleteLater;
    ~DataManager() {
        for (auto data : std::as_const(dataList)) {
            Derive::destroy(data->data);
        }
    }

    QList<std::shared_ptr<Data>> dataList;
    QRunnable *cleanJob = nullptr;
};

class Q_DECL_HIDDEN RhiTextureManager : public DataManager<RhiTextureManager, QRhiTexture, QRhiTexture::Format, const QSize&>
{
    Q_OBJECT

    friend class DataManager;

    RhiTextureManager(QQuickWindow *owner)
        : DataManager<RhiTextureManager, QRhiTexture, QRhiTexture::Format, const QSize&>(owner) {
        Q_ASSERT(owner->findChildren<RhiTextureManager*>(Qt::FindDirectChildrenOnly).size() == 1);
    }

    static bool check(QRhiTexture *texture, QRhiTexture::Format format, const QSize &size) {
        return texture->format() == format && texture->pixelSize() == size;
    }

    QRhiTexture *create(QRhiTexture::Format format, const QSize &size) {
        auto texture = owner()->rhi()->newTexture(format, size, 1, QRhiTexture::RenderTarget);
        if  (!texture->create()) {
            delete texture;
            return nullptr;
        }

        return texture;
    }

    static void destroy(QRhiTexture *texture) {
        texture->deleteLater();
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

        renderer->m_is_rendering = true;
        renderer->preprocess();

        return true;
    }

    bool render(qreal oldDPR, QRhiCommandBuffer* &oldCB, bool forceDepthTest = false) {
        Q_ASSERT(renderer->m_is_rendering);
        renderer->render();
        renderer->m_is_rendering = false;
        renderer->m_changed_emitted = false;

        context->prepareSync(oldDPR, oldCB, graphicsConfiguration());
        bool ok = false;

        do {
            if (forceDepthTest && Q_LIKELY(isBatchRenderer)) {
                auto batchRenderer = static_cast<QSGBatchRenderer::Renderer*>(renderer);
                if (auto sm = batchRenderer->m_shaderManager) {
                    if (Q_LIKELY(!sm->pipelineCache.isEmpty())) {
                        QVector<std::pair<QRhiGraphicsPipeline*, bool>> tmp;
                        tmp.reserve(sm->pipelineCache.size());

                        for (auto pipeline : std::as_const(sm->pipelineCache)) {
                            tmp.append({pipeline, pipeline->hasDepthTest()});
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

        context = QQuickWindowPrivate::get(owner)->context;
        // Don't use RenderMode2D, when use this renderer on an exists renderTarget
        // we need to ensure the renderer don't overwrite the depth buffer, because
        // the exists renderTarget is using in the other QSGRenderer, maybe that
        // renderer is using depth test, and the other QSGRenderer is not finished render.
        // For an example: RhiNode to render its content nodes on an exists renderTarget.
        renderer = context->createRenderer(QSGRendererInterface::RenderMode2DNoDepthBuffer);
        isBatchRenderer = dynamic_cast<QSGBatchRenderer::Renderer*>(renderer);
    }

    ~RhiManager() {
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
        QRhi *rhi;
        QOffscreenSurface *offscreenSurface;
        bool own;
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
        if (Q_UNLIKELY(renderData && !contentNode))
            return (RenderTargetState | ViewportState);

        return {0};
    }

    // ###: Should disable DepthAwareRendering here, because
    // if enable depth test, Qt will render the opaque nodes first,
    // maybe an opaque node's z-order is higher than the render node,
    // will get the wrong texture when copy texture in RhiNode::render.
    // But the opaque node will be cover the wrong area to the
    // RhiNode::renderTarget after RhiNode::render, but if you using
    // MultiEffect's blur effect for the copied texture, the blur result
    // will contains the opaque node's contains. Fortunately, this kind
    // of problem is not very obvious.
    //
    // We should fix this bug in Qt in the future.
    RenderingFlags flags() const override {
        if (Q_UNLIKELY(!contentNode))
            return BoundedRectRendering | DepthAwareRendering;

        return DepthAwareRendering;
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

        const auto currentRenderer = maybeBufferRenderer();
        // TODO: Apple viewport to matrix, needs get QSGRenderer
        renderMatrix = currentRenderer
                           ? currentRenderer->currentWorldTransform() * (*this->matrix())
                           : *this->matrix();
        devicePixelRatio = effectiveDevicePixelRatio();
        const auto oldManager = manager;
        manager = RhiTextureManager::resolve(manager, window);

        if (oldManager != manager) {
            sgTexture()->setTexture(nullptr);
            if (oldManager)
                oldManager->release(texture);
            texture.reset();
        }

        Q_ASSERT(ct->rhi() == window->rhi());
        rhi = rhi->resolve(rhi, window);
        Q_ASSERT(rhi);

        const bool hasRotation = renderMatrix.flags().testAnyFlags(QMatrix4x4::Rotation2D | QMatrix4x4::Rotation);
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
            renderData.reset();

            QSizeF size = renderMatrix.mapRect(m_rect).size() * devicePixelRatio;
            if (!size.isValid()) {
                reset();
                return;
            }

            pixelSize = size.toSize();
        }

        texture = manager->resolve(texture, ct->format(), pixelSize);
        if (Q_UNLIKELY(texture.expired())) {
            reset();
            return;
        }
        auto texture = this->texture.lock();
        Q_ASSERT(texture->data);

        if (renderData) {
            if (!renderData->rt || sgTexture()->rhiTexture() != texture->data) {
                QRhiTextureRenderTargetDescription rtDesc(texture->data);
                const auto flags = QRhiTextureRenderTarget::PreserveColorContents
                    | QRhiTextureRenderTarget::PreserveDepthStencilContents;
                auto newRT = rhi->rhi()->newTextureRenderTarget(rtDesc, flags);
                newRT->setRenderPassDescriptor(newRT->newCompatibleRenderPassDescriptor());
                if (!newRT->create()) {
                    delete newRT;
                    return;
                }

                renderData->rt.reset(newRT);
            }
        }

        if (m_content) {
            auto rootNode = WQmlHelper::getRootNode(m_content);
            if (rootNode && rootNode->firstChild()) {
                contentNode = rootNode;
            }
        }
    }

    void render([[maybe_unused]] const RenderState *state) override {

        auto texture = this->texture.lock();
        if (Q_UNLIKELY(!texture))
            return;

        auto ct = currentRenderTexture();
        Q_ASSERT(ct);

        if (renderData) {
            renderData->texture.setTexture(ct);
            renderData->texture.setTextureSize(ct->pixelSize());

            const QPointF sourcePos = renderMatrix.map(m_rect.topLeft());
            renderData->imageNode->setRect(QRectF(-(devicePixelRatio - 1) * sourcePos, ct->pixelSize()));

            rhi->sync(texture->data->pixelSize(), &renderData->rootNode, renderMatrix.inverted(), {}, nullptr,
                      {texture->data->pixelSize().width() / float(m_rect.width() * devicePixelRatio),
                       texture->data->pixelSize().height() / float(m_rect.height() * devicePixelRatio)});
            rhi->render(renderData->rt.get());
        } else {
            auto rhi = this->rhi->rhi();
            QPointF sourcePos = renderMatrix.map(m_rect.topLeft()) * devicePixelRatio;

            auto rub = rhi->nextResourceUpdateBatch();
            QRhiTextureCopyDescription desc;
            desc.setPixelSize(texture->data->pixelSize());
            desc.setSourceTopLeft(sourcePos.toPoint());
            rub->copyTexture(texture->data, ct, desc);

            QRhiCommandBuffer *cb = nullptr;
            if (rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
                return;
            Q_ASSERT(cb);

            // TODO: needs vkCmdPipelineBarrier?
            cb->resourceUpdate(rub);
            rhi->endOffscreenFrame();
        }

        if (sgTexture()->rhiTexture() != texture->data)
            sgTexture()->setTexture(texture->data);
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
                                   && currentRenderer->currentBatchRenderer()->useDepthBuffer();

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
        manager = QImageManager::resolve(manager, window);

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
    return BoundedRectRendering;
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
