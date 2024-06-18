// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputrenderwindow.h"
#include "woutput.h"
#include "woutputhelper.h"
#include "wrenderhelper.h"
#include "wbackend.h"
#include "woutputviewport.h"
#include "woutputviewport_p.h"
#include "wtools.h"
#include "wquickbackend_p.h"
#include "wwaylandcompositor_p.h"
#include "wqmlhelper_p.h"
#include "woutputlayer.h"
#include "wbufferrenderer_p.h"
#include "wquicktextureproxy.h"

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
#include <qwtexture.h>
#include <qwswapchain.h>
#include <qwsignalconnector.h>

#include <QOffscreenSurface>
#include <QQuickRenderControl>
#include <QOpenGLFunctions>
#include <memory>

#define protected public
#define private public
#include <private/qsgrenderer_p.h>
#include <private/qsgsoftwarerenderer_p.h>
#include <private/qquickanimatorcontroller_p.h>
#undef protected
#undef private
#include <private/qquickwindow_p.h>
#include <private/qquickrendercontrol_p.h>
#include <private/qquickwindow_p.h>
#include <private/qrhi_p.h>
#include <private/qsgrhisupport_p.h>
#include <private/qquicktranslate_p.h>
#include <private/qquickitem_p.h>
#include <private/qsgabstractrenderer_p.h>
#include <private/qsgrenderer_p.h>
#include <private/qpainter_p.h>
#include <private/qsgdefaultrendercontext_p.h>

extern "C" {
#define static
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/interface.h>
#include <wlr/render/gles2.h>
#include <wlr/types/wlr_output_layer.h>
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

inline static void resetGlState()
{
#ifndef QT_NO_OPENGL
    // Clear OpenGL state for wlroots, the states is set by Qt, But it is will
    // effect to wlroots's gles renderer.
    if (WRenderHelper::getGraphicsApi() == QSGRendererInterface::OpenGL) {
        // If not reset, you will get a warning from Mesa(enable MESA_DEBUG):
        // Mesa: warning: Received negative int32 vertex buffer offset. (driver limitation)
        glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_NONE);
        glDisable(GL_DEPTH_TEST);
    }
#endif
}

// TODO: move to qwlroots
class QWOutputLayer : public QObject
{
    Q_OBJECT
public:
    ~QWOutputLayer();

    inline wlr_output_layer *handle() const {
        return m_handle;
    }

    static QWOutputLayer *create(QWOutput *output, QObject *parent = nullptr);

Q_SIGNALS:
    void feedback();

private:
    explicit QWOutputLayer(wlr_output_layer *handle, QWOutput *output, QObject *parent = nullptr);
    void onFeedback(wlr_output_layer_feedback_event *event);

    wlr_output_layer *m_handle;
    QWSignalConnector sc;
};

class OutputLayer;
class OutputHelper : public WOutputHelper
{
    friend class WOutputRenderWindowPrivate;
public:
    struct LayerData {
        LayerData(OutputLayer *l, QWOutputLayer *layer)
            : layer(l)
            , wlrLayer(layer)
            , contentsIsDirty(true)
            , needsFlush(false)
        {

        }
        ~LayerData() {
            if (renderer)
                renderer->deleteLater();
            wlrLayer->deleteLater();
        }

        OutputLayer *layer;
        QWOutputLayer *wlrLayer;
        QPointer<WBufferRenderer> renderer;

        // dirty state
        uint contentsIsDirty:1;
        uint needsFlush:1;
        // end

        QRectF mapRect;
        QMatrix4x4 renderMatrix;
    };

    OutputHelper(WOutputViewport *output, WOutputRenderWindow *parent)
        : WOutputHelper(output->output(), parent)
        , m_output(output)
    {

    }

    ~OutputHelper()
    {
        cleanLayerCompositor();
        qDeleteAll(m_layers);
    }

    inline void init() {
        connect(this, &OutputHelper::requestRender, renderWindow(), qOverload<>(&WOutputRenderWindow::render));
        connect(this, &OutputHelper::damaged, renderWindow(), &WOutputRenderWindow::scheduleRender);
        connect(output(), &WOutputViewport::layerFlagsChanged, renderWindow(), &WOutputRenderWindow::scheduleRender);
        // TODO: pre update scale after WOutputHelper::setScale
        output()->output()->safeConnect(&WOutput::scaleChanged, this, &OutputHelper::updateSceneDPR);
    }

    inline QWOutput *qwoutput() const {
        return output()->output()->handle();
    }

    inline WOutputRenderWindow *renderWindow() const {
        return static_cast<WOutputRenderWindow*>(parent());
    }
    inline WOutputRenderWindowPrivate *renderWindowD() const;

    inline WOutputViewport *output() const {
        return m_output;
    }

    inline WBufferRenderer *bufferRenderer() const {
        return WOutputViewportPrivate::get(m_output)->bufferRenderer;
    }

    inline WBufferRenderer *bufferRenderer2() const {
        Q_ASSERT(m_output2);
        return WOutputViewportPrivate::get(m_output2)->bufferRenderer;
    }

    inline const QList<LayerData*> &layers() const {
        return m_layers;
    }

    inline void invalidate() {
        m_output = nullptr;
    }

    inline qreal devicePixelRatio() const {
        return m_output->devicePixelRatio();
    }

    void updateSceneDPR();

    int indexOfLayer(OutputLayer *layer) const;
    bool attachLayer(OutputLayer *layer);
    void detachLayer(OutputLayer *layer);
    void sortLayers();
    void cleanLayerCompositor();

    inline QWBuffer *beginRender(WBufferRenderer *renderer,
                                 const QSize &pixelSize, uint32_t format,
                                 WBufferRenderer::RenderFlags flags);
    inline void render(WBufferRenderer *renderer, int sourceIndex,
                       const QMatrix4x4 &renderMatrix, bool preserveColorContents);
    void beforeRender();
    WBufferRenderer *afterRender();
    WBufferRenderer *compositeLayers(const QVector<LayerData*> layers);
    bool commit(WBufferRenderer *buffer);

private:
    WOutputViewport *m_output = nullptr;
    WBufferRenderer *m_bufferRenderer = nullptr;
    QList<LayerData*> m_layers;
    WBufferRenderer *m_lastCommitBuffer = nullptr;

    // for compositeLayers
    QPointer<WOutputViewport> m_output2;
    QPointer<QQuickItem> m_layerPorxyContainer;
    QList<QPointer<WQuickTextureProxy>> m_layerProxys;
};

QWOutputLayer::QWOutputLayer(wlr_output_layer *handle, QWOutput *output, QObject *parent)
    : QObject(parent)
    , m_handle(handle)
{
    // The layer will destroy in wlr_output_destroy()
    connect(output, &QWOutput::beforeDestroy, this, [this] {
        sc.invalidate();
        m_handle = nullptr;
    });

    sc.connect(&handle->events.feedback, this, &QWOutputLayer::onFeedback);
}

void QWOutputLayer::onFeedback(wlr_output_layer_feedback_event *event)
{
    Q_UNUSED(event)
    // TODO
}

QWOutputLayer::~QWOutputLayer()
{
    wlr_output_layer_destroy(m_handle);
}

QWOutputLayer *QWOutputLayer::create(QWOutput *output, QObject *parent)
{
    auto handle = wlr_output_layer_create(output->handle());
    return handle ? new QWOutputLayer(handle, output, parent) : nullptr;
}

class OutputLayer
{
    friend class OutputHelper;
    friend class WOutputRenderWindow;
    friend class WOutputRenderWindowPrivate;
public:
    OutputLayer(WOutputLayer *layer)
        : layer(layer) {}

private:
    WOutputLayer *layer;
    QList<WOutputViewport*> outputs;
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

static QEvent::Type doRenderEventType = static_cast<QEvent::Type>(QEvent::registerEventType());
class WOutputRenderWindowPrivate : public QQuickWindowPrivate
{
public:
    WOutputRenderWindowPrivate(WOutputRenderWindow *)
        : QQuickWindowPrivate() {
    }
    ~WOutputRenderWindowPrivate() {
        qDeleteAll(layers);
    }

    static inline WOutputRenderWindowPrivate *get(WOutputRenderWindow *qq) {
        return qq->d_func();
    }

    int indexOfOutputHelper(WOutputViewport *output) const;
    inline OutputHelper *getOutputHelper(WOutputViewport *output) const {
        int index = indexOfOutputHelper(output);
        if (index >= 0)
            return outputs.at(index);
        return nullptr;
    }

    int indexOfOutputLayer(WOutputLayer *layer) const;
    inline OutputLayer *ensureOutputLayer(WOutputLayer *layer) {
        int index = indexOfOutputLayer(layer);
        if (index >= 0)
            return layers.at(index);
        layers.append(new OutputLayer(layer));
        return layers.last();
    }

    inline RenderControl *rc() const {
        return static_cast<RenderControl*>(q_func()->renderControl());
    }

    inline bool isInitialized() const {
        return rc()->m_renderWindow;
    }

    inline void setSceneDevicePixelRatio(qreal ratio) {
        static_cast<QWlrootsRenderWindow*>(platformWindow)->setDevicePixelRatio(ratio);
    }

    QSGRendererInterface::GraphicsApi graphicsApi() const;
    void init();
    void init(OutputHelper *helper);
    bool initRCWithRhi();
    void updateSceneDPR();

    QVector<std::pair<OutputHelper *, WBufferRenderer *>>
    doRenderOutputs(const QList<OutputHelper *> &outputs, bool forceRender);
    void doRender(const QList<OutputHelper*> &outputs, bool forceRender, bool doCommit);
    inline void doRender() {
        doRender(outputs, false, true);
    }

    inline void pushRenderer(WBufferRenderer *renderer) {
        rendererList.push(renderer);
    }

    inline void scheduleDoRender() {
        if (!isInitialized())
            return; // Not initialized

        if (inRendering)
            return;

        QCoreApplication::postEvent(q_func(), new QEvent(doRenderEventType));
    }

    void sortLayers();

    Q_DECLARE_PUBLIC(WOutputRenderWindow)

    bool componentCompleted = true;
    bool inRendering = false;
    WWaylandCompositor *compositor = nullptr;

    QList<OutputHelper*> outputs;
    QList<OutputLayer*> layers;

    QOpenGLContext *glContext = nullptr;
#ifdef ENABLE_VULKAN_RENDER
    QScopedPointer<QVulkanInstance> vkInstance;
#endif

    QStack<WBufferRenderer*> rendererList;
};

WOutputRenderWindowPrivate *OutputHelper::renderWindowD() const
{
    return WOutputRenderWindowPrivate::get(renderWindow());
}

void OutputHelper::updateSceneDPR()
{
    WOutputRenderWindowPrivate::get(renderWindow())->updateSceneDPR();
}

int OutputHelper::indexOfLayer(OutputLayer *layer) const
{
    for (int i = 0; i < m_layers.count(); ++i)
        if (m_layers.at(i)->layer == layer)
            return i;

    return -1;
}

bool OutputHelper::attachLayer(OutputLayer *layer)
{
    Q_ASSERT(indexOfLayer(layer) < 0);
    auto qwlayer = QWOutputLayer::create(qwoutput(), this);
    if (!qwlayer)
        return false;

    m_layers.append(new LayerData(layer, qwlayer));
    connect(layer->layer, &WOutputLayer::zChanged, this, &OutputHelper::sortLayers);
    sortLayers();

    return true;
}

void OutputHelper::detachLayer(OutputLayer *layer)
{
    int index = indexOfLayer(layer);
    Q_ASSERT(index >= 0);

    delete m_layers.takeAt(index);
}

void OutputHelper::sortLayers()
{
    if (m_layers.size() < 2)
        return;

    std::sort(m_layers.begin(), m_layers.end(),
              [] (const LayerData *l1,
                  const LayerData *l2) {
        return l2->layer->layer->z() > l1->layer->layer->z();
    });
}

void OutputHelper::cleanLayerCompositor()
{
    QList<QPointer<WQuickTextureProxy>> tmpList;
    std::swap(m_layerProxys, tmpList);

    for (auto proxy : tmpList) {
        if (!proxy)
            continue;

        WBufferRenderer *source = qobject_cast<WBufferRenderer*>(proxy->sourceItem());
        proxy->setSourceItem(nullptr);

        if (source) {
            source->setForceCacheBuffer(false);
            source->resetTextureProvider();
        }
    }

    if (m_output2) {
        m_output2->deleteLater();
        m_output2 = nullptr;
    }

    if (m_output) {
        auto d = WOutputViewportPrivate::get(m_output);
        if (!d->inDestructor)
            d->setExtraRenderSource(nullptr);
    }

    if (m_layerPorxyContainer) {
        m_layerPorxyContainer->deleteLater();
        m_layerPorxyContainer = nullptr;
    }
}

QWBuffer *OutputHelper::beginRender(WBufferRenderer *renderer,
                                    const QSize &pixelSize, uint32_t format,
                                    WBufferRenderer::RenderFlags flags)
{
    return renderer->beginRender(pixelSize, devicePixelRatio(), format, flags);
}

void OutputHelper::render(WBufferRenderer *renderer, int sourceIndex,
                          const QMatrix4x4 &renderMatrix, bool preserveColorContents)
{
    renderWindowD()->pushRenderer(renderer);
    renderer->render(sourceIndex, renderMatrix, preserveColorContents);
}

void OutputHelper::beforeRender()
{
    const auto layersFlags = output()->layerFlags();
    if (layersFlags.testFlag(WOutputViewport::LayerFlag::AlwaysRejected)) {
        for (auto i : m_layers)
            i->layer->layer->setAccepted(false);
    } else if (layersFlags.testFlag(WOutputViewport::LayerFlag::AlwaysAccepted)) {
        for (auto i : m_layers)
            i->layer->layer->setAccepted(true);
    }
}

struct Q_DECL_HIDDEN QScopedPointerWlArrayDeleter {
    static inline void cleanup(wl_array *pointer) {
        if (pointer)
            wl_array_release(pointer);
        delete pointer;
    }
};
typedef QScopedPointer<wl_array, QScopedPointerWlArrayDeleter> wl_array_pointer;

WBufferRenderer *OutputHelper::afterRender()
{
    const auto layersFlags = output()->layerFlags();
    if (m_layers.isEmpty() || layersFlags.testFlag(WOutputViewport::LayerFlag::AlwaysRejected)) {
        cleanLayerCompositor();
        return bufferRenderer();
    }

    // update layers
    wlr_output_layer_state_array layers;
    QList<LayerData*> datas;
    layers.reserve(m_layers.size());
    datas.reserve(m_layers.size());

    for (auto i : m_layers) {
        if (!i->layer->layer->parent()->isVisible())
            continue;

        // TODO: Should continue if the render format is changed
        if (!i->layer->layer->isAccepted())
            continue;

        auto source = i->layer->layer->parent();
        if (!i->renderer) {
            i->renderer = new WBufferRenderer(source);
            QQuickItemPrivate::get(i->renderer)->anchors()->setFill(source);
            i->renderer->setSourceList({source}, false);
            i->renderer->setOutput(output()->output());

            connect(i->renderer, &WBufferRenderer::sceneGraphChanged, this, [i] {
                i->contentsIsDirty = true;
            });
        }

        const auto sourceD = QQuickItemPrivate::get(source);
        const auto viewportD = QQuickItemPrivate::get(output());
        QRectF mapRect = QRectF(QPointF(0, 0), source->size());
        const QMatrix4x4 mapMatrix = sourceD->itemToWindowTransform() * viewportD->windowToItemTransform();
        const qreal dpr = devicePixelRatio();
        mapRect = mapMatrix.mapRect(mapRect);
        mapRect.moveTo(mapRect.topLeft() * dpr);

        if (i->layer->layer->flags().testFlag(WOutputLayer::SizeFollowTransformation)) {
            mapRect.setSize(mapRect.size() * dpr);
        } else {
            QSizeF size = source->size() * dpr;

            if (i->layer->layer->flags().testFlag(WOutputLayer::SizeFollowItemTransformation)) {
                size = sourceD->itemNode()->matrix().mapRect(QRectF(QPointF(0, 0), size)).size();
            }

            if (output()->output()->orientation() % 2 != 0) {
                std::swap(size.rwidth(), size.rheight());
            }

            mapRect.setSize(size);
        }

        std::swap(i->mapRect, mapRect);
        const QRectF viewportRect(QPointF(0, 0), output()->output()->size());

        if ((viewportRect & i->mapRect).isEmpty()) {
            continue;
        }

        auto mapToViewportMatrixData = mapMatrix.constData();
        const QMatrix4x4 renderMatrix {
            mapToViewportMatrixData[0],
            mapToViewportMatrixData[1 * 4 + 0],
            mapToViewportMatrixData[2 * 4 + 0],
            mapToViewportMatrixData[3 * 4 + 0] - static_cast<float>(i->mapRect.x() / dpr),
            mapToViewportMatrixData[1],
            mapToViewportMatrixData[1 * 4 + 1],
            mapToViewportMatrixData[2 * 4 + 1],
            mapToViewportMatrixData[3 * 4 + 1] - static_cast<float>(i->mapRect.y() / dpr),
            mapToViewportMatrixData[2],
            mapToViewportMatrixData[1 * 4 + 2],
            mapToViewportMatrixData[2 * 4 + 2],
            mapToViewportMatrixData[3 * 4 + 2],
            mapToViewportMatrixData[3],
            mapToViewportMatrixData[1 * 4 + 3],
            mapToViewportMatrixData[2 * 4 + 3],
            mapToViewportMatrixData[3 * 4 + 3],
        };

        auto buffer = i->renderer->lastBuffer();
        bool isNewBuffer = true;
        const bool needsFullUpdate = i->mapRect.size() != mapRect.size()
                                     || i->renderMatrix != renderMatrix;

        if (!buffer || i->contentsIsDirty || needsFullUpdate) {
            const QSize pixelSize(qCeil(i->mapRect.width()), qCeil(i->mapRect.height()));
            const bool alpha = !i->layer->layer->flags().testFlag(WOutputLayer::NoAlpha);
            buffer = beginRender(i->renderer, pixelSize,
                                 // TODO: Allows control format by WOutputLayer
                                 alpha ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888,
                                 WBufferRenderer::DontConfigureSwapchain);
            if (buffer) {
                render(i->renderer, 0, renderMatrix,
                       i->layer->layer->flags().testFlag(WOutputLayer::PreserveColorContents));
            }
        } else {
            isNewBuffer = false;
        }

        i->contentsIsDirty = false;
        i->renderMatrix = renderMatrix;

        if (!buffer)
            continue;

        layers.append({
            .layer = i->wlrLayer->handle(),
            .buffer = buffer->handle(),
            .dst_box = {
                .x = qRound(i->mapRect.x()),
                .y = qRound(i->mapRect.y()),
                .width = qRound(i->mapRect.width()),
                .height = qRound(i->mapRect.height()),
            },
            .damage = &i->renderer->damageRing()->handle()->current
        });

        if (isNewBuffer) {
            Q_ASSERT(buffer);
            i->renderer->endRender();
        }

        datas.append(i);
    }

    if (layers.isEmpty()) {
        cleanLayerCompositor();
        return bufferRenderer();
    }

    if (output()->offscreen()) {
        cleanLayerCompositor();
        return bufferRenderer();
    }

    bool ok = WOutputHelper::testCommit(bufferRenderer()->currentBuffer(), layers);

    QList<LayerData*> needsCompositeLayers;
    if (!ok) {
        needsCompositeLayers = m_layers;
    } else {
        int needsCompositeIndex = -1;
        {
            for (int i = 0; i < layers.length(); ++i) {
                const auto &state = layers.at(i);
                if (state.accepted) {
                    datas.at(i)->layer->layer->setAccepted(true);
                } else if (state.buffer) {
                    needsCompositeIndex = i;
                }
            }
        }

        if (needsCompositeIndex < datas.size() - 1) {
            datas.resize(++needsCompositeIndex);
            layers.remove(0, datas.size());
            Q_ASSERT(!layers.isEmpty());
            setLayers(layers);
        }

        needsCompositeLayers = datas;
    }

    if (!layersFlags.testFlag(WOutputViewport::LayerFlag::AlwaysAccepted)) {
        for (auto i : needsCompositeLayers)
            i->layer->layer->setAccepted(false);
        return bufferRenderer();
    }

    if (needsCompositeLayers.isEmpty()) {
        cleanLayerCompositor();
        return bufferRenderer();
    }

    return compositeLayers(needsCompositeLayers);
}

#define PRIVATE_WOutputViewport "__private_WOutputViewport"
WBufferRenderer *OutputHelper::compositeLayers(const QList<LayerData*> layers)
{
    Q_ASSERT(!layers.isEmpty());

    const bool usingShadowRenderer = this->output()->layerFlags().testFlag(WOutputViewport::LayerFlag::UsingShadowBufferOnComposite)
                                     // TODO: Support preserveColorContents in Qt in QSGSoftwareRenderer
                                     || dynamic_cast<QSGSoftwareRenderer*>(renderWindowD()->renderer);

    if (!m_layerPorxyContainer) {
        m_layerPorxyContainer = new QQuickItem(renderWindow()->contentItem());
    }

    WOutputViewport *output;

    if (usingShadowRenderer) {
        if (!m_output2) {
            m_output2 = new WOutputViewport(m_output);
            m_output2->setObjectName(PRIVATE_WOutputViewport);
            m_output2->setOutput(m_output->output());
            bufferRenderer2()->setSourceList({m_layerPorxyContainer.get()}, true);
        }

        m_output2->setSize(m_output->size());
        m_output2->setDevicePixelRatio(m_output->devicePixelRatio());
        output = m_output2;

        if (m_layerProxys.size() <= layers.size())
            m_layerProxys.reserve(layers.size() + 1);

        if (m_layerProxys.isEmpty())
            m_layerProxys.append(new WQuickTextureProxy(m_layerPorxyContainer));

        auto outputProxy = m_layerProxys.first();
        bufferRenderer()->setForceCacheBuffer(true);
        outputProxy->setSourceItem(bufferRenderer());
        outputProxy->setSize(output->size());
    } else {
        output = m_output;

        if (m_layerProxys.size() < layers.size())
            m_layerProxys.reserve(layers.size());

        WOutputViewportPrivate::get(output)->setExtraRenderSource(m_layerPorxyContainer);
    }

    m_layerPorxyContainer->setSize(output->size());
    const qreal dpr = devicePixelRatio();

    for (int i = 0; i < layers.count(); ++i) {
        const int j = i + (usingShadowRenderer ? 1 : 0);
        WQuickTextureProxy *proxy = nullptr;
        if (j < m_layerProxys.size()) {
            proxy = m_layerProxys.at(j);
        } else {
            proxy = new WQuickTextureProxy(m_layerPorxyContainer);
            m_layerProxys.append(proxy);
        }

        LayerData *layer = layers.at(i);
        layer->layer->layer->setAccepted(true);
        layer->renderer->setForceCacheBuffer(true);
        proxy->setSourceItem(layer->renderer);
        proxy->setPosition(layer->mapRect.topLeft() / dpr);
        proxy->setSize(layer->mapRect.size());
        proxy->setZ(layer->layer->layer->z());
    }

    // Clean
    for (int i = layers.count() + (usingShadowRenderer ? 1 : 0); i < m_layerProxys.count(); ++i) {
        auto proxy = m_layerProxys.takeAt(i);
        proxy->setVisible(false);
        proxy->deleteLater();
    }

    renderWindow()->renderControl()->sync();

    if (usingShadowRenderer) {
        const bool ok = beginRender(bufferRenderer2(), m_output->output()->size(),
                                    qwoutput()->handle()->render_format,
                                    WBufferRenderer::RedirectOpenGLContextDefaultFrameBufferObject);

        if (ok) {
            bufferRenderer()->endRender();
            render(bufferRenderer2(), 0, {}, true);

            return bufferRenderer2();
        }
    } else {
        render(bufferRenderer(), 1, {}, true);
    }

    return bufferRenderer();
}

bool OutputHelper::commit(WBufferRenderer *buffer)
{
    if (output()->offscreen())
        return true;

    if (!buffer || !buffer->currentBuffer()) {
        Q_ASSERT(!this->buffer());
        return WOutputHelper::commit();
    }

    setBuffer(buffer->currentBuffer());

    if (m_lastCommitBuffer == buffer) {
        if (pixman_region32_not_empty(&buffer->damageRing()->handle()->current))
            setDamage(&buffer->damageRing()->handle()->current);
    }

    m_lastCommitBuffer = buffer;

    return WOutputHelper::commit();
}

int WOutputRenderWindowPrivate::indexOfOutputHelper(WOutputViewport *output) const
{
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i)->output() == output) {
            return i;
        }
    }

    return -1;
}

int WOutputRenderWindowPrivate::indexOfOutputLayer(WOutputLayer *layer) const
{
    for (int i = 0; i < layers.size(); ++i) {
        if (layers.at(i)->layer == layer) {
            return i;
        }
    }

    return -1;
}

QSGRendererInterface::GraphicsApi WOutputRenderWindowPrivate::graphicsApi() const
{
    auto api = WRenderHelper::getGraphicsApi(rc());
    Q_ASSERT(api == WRenderHelper::getGraphicsApi());
    return api;
}

void WOutputRenderWindowPrivate::init()
{
    Q_ASSERT(compositor);
    Q_Q(WOutputRenderWindow);

    if (QSGRendererInterface::isApiRhiBased(graphicsApi()))
        initRCWithRhi();
    Q_ASSERT(context);
    q->create();
    rc()->m_renderWindow = q;

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

    for (auto layer : layers) {
        if (layer->outputs.contains(helper->output())) {
            helper->attachLayer(layer);
        }
    }
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
    Q_ASSERT(!swapchain);

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

QVector<std::pair<OutputHelper*, WBufferRenderer*>>
WOutputRenderWindowPrivate::doRenderOutputs(const QList<OutputHelper*> &outputs, bool forceRender)
{
    QVector<OutputHelper*> renderResults;
    renderResults.reserve(outputs.size());
    for (OutputHelper *helper : outputs) {
        if (Q_LIKELY(!forceRender)) {
            if (!helper->renderable()
                || Q_UNLIKELY(!WOutputViewportPrivate::get(helper->output())->renderable())
                || !helper->output()->output()->isEnabled())
                continue;

            if (!helper->contentIsDirty()) {
                if (helper->needsFrame())
                    renderResults.append(helper);
                continue;
            }
        }

        Q_ASSERT(helper->output()->output()->scale() <= helper->output()->devicePixelRatio());

        const auto &format = helper->qwoutput()->handle()->render_format;
        QMatrix4x4 renderMatrix;

        auto viewportMatrix = QQuickItemPrivate::get(helper->output())->itemNode()->matrix().inverted();
        if (helper->output()->input() == helper->output()) {
            auto mapToViewportMatrixData = viewportMatrix.constData();
            // Remove the x,y translate, if the input item is it self, only apply scale/rotation to render.
            const QMatrix4x4 tmpMatrix {
                mapToViewportMatrixData[0],
                mapToViewportMatrixData[1 * 4 + 0],
                mapToViewportMatrixData[2 * 4 + 0],
                0,
                mapToViewportMatrixData[1],
                mapToViewportMatrixData[1 * 4 + 1],
                mapToViewportMatrixData[2 * 4 + 1],
                0,
                mapToViewportMatrixData[2],
                mapToViewportMatrixData[1 * 4 + 2],
                mapToViewportMatrixData[2 * 4 + 2],
                mapToViewportMatrixData[3 * 4 + 2],
                mapToViewportMatrixData[3],
                mapToViewportMatrixData[1 * 4 + 3],
                mapToViewportMatrixData[2 * 4 + 3],
                mapToViewportMatrixData[3 * 4 + 3],
            };
            renderMatrix = tmpMatrix;
        } else if (auto inputItem = helper->output()->input()) {
            QMatrix4x4 matrix = QQuickItemPrivate::get(helper->output()->parentItem())->itemToWindowTransform();
            matrix *= QQuickItemPrivate::get(inputItem)->windowToItemTransform();
            renderMatrix = viewportMatrix * matrix.inverted();
        } else { // the input item is window's contentItem
            QMatrix4x4 parentMatrix = QQuickItemPrivate::get(helper->output()->parentItem())->itemToWindowTransform().inverted();
            renderMatrix = viewportMatrix * parentMatrix;
        }

        QWBuffer *buffer = helper->beginRender(helper->bufferRenderer(), helper->output()->output()->size(), format,
                                               WBufferRenderer::RedirectOpenGLContextDefaultFrameBufferObject);
        Q_ASSERT(buffer == helper->bufferRenderer()->currentBuffer());
        if (buffer) {
            helper->render(helper->bufferRenderer(), 0, renderMatrix, helper->output()->preserveColorContents());
        }
        renderResults.append(helper);
    }

    QVector<std::pair<OutputHelper*, WBufferRenderer*>> needsCommit;
    needsCommit.reserve(renderResults.size());
    for (auto helper : renderResults) {
        auto bufferRenderer = helper->afterRender();
        if (bufferRenderer)
            needsCommit.append({helper, bufferRenderer});
    }

    rendererList.clear();

    return needsCommit;
}

// ###: QQuickAnimatorController::advance symbol not export
static void QQuickAnimatorController_advance(QQuickAnimatorController *ac)
{
    bool running = false;
    for (const QSharedPointer<QAbstractAnimationJob> &job : std::as_const(ac->m_animationRoots)) {
        if (job->isRunning()) {
            running = true;
            break;
        }
    }

    for (QQuickAnimatorJob *job : std::as_const(ac->m_runningAnimators))
        job->commit();

    if (running)
        ac->m_window->update();
}

void WOutputRenderWindowPrivate::doRender(const QList<OutputHelper *> &outputs,
                                          bool forceRender, bool doCommit)
{
    Q_ASSERT(rendererList.isEmpty());
    Q_ASSERT(!inRendering);
    inRendering = true;

    rc()->polishItems();
    if (QSGRendererInterface::isApiRhiBased(WRenderHelper::getGraphicsApi()))
        rc()->beginFrame();
    rc()->sync();

    QQuickAnimatorController_advance(animationController.get());
    Q_EMIT q_func()->beforeRendering();
    runAndClearJobs(&beforeRenderingJobs);

    for (OutputHelper *helper : outputs)
        helper->beforeRender();

    auto needsCommit = doRenderOutputs(outputs, forceRender);

    Q_EMIT q_func()->afterRendering();
    runAndClearJobs(&afterRenderingJobs);

    if (QSGRendererInterface::isApiRhiBased(WRenderHelper::getGraphicsApi()))
        rc()->endFrame();

    if (doCommit) {
        for (auto i : needsCommit) {
            bool ok = i.first->commit(i.second);

            if (i.second->currentBuffer()) {
                i.second->endRender();
            }

            i.first->resetState(ok);
        }
    }

    resetGlState();

    // On Intel&Nvidia multi-GPU environment, wlroots using Intel card do render for all
    // outputs, and blit nvidia's output buffer in drm_connector_state_update_primary_fb,
    // the 'blit' behavior will make EGL context to Nvidia renderer. So must done current
    // OpenGL context here in order to ensure QtQuick always make EGL context to Intel
    // renderer before next frame.
    if (glContext)
        glContext->doneCurrent();

    inRendering = false;
}

// TODO: Support QWindow::setCursor
WOutputRenderWindow::WOutputRenderWindow(QObject *parent)
    : QQuickWindow(*new WOutputRenderWindowPrivate(this), new RenderControl())
{
    setObjectName(QW::RenderWindow::id());

    if (parent)
        QObject::setParent(parent);

    connect(contentItem(), &QQuickItem::widthChanged, this, &WOutputRenderWindow::widthChanged);
    connect(contentItem(), &QQuickItem::heightChanged, this, &WOutputRenderWindow::heightChanged);
    // renderwindow inherits qquickwindow, default no focusscope-isolation & no focus on contentItem(QQuickRootItem) when startup
    // setFocus(true) to deliver focus to children on startup, 
    // set focusscope to persist & restore in-scope focusItem, even after other item takes away the focus
    // see [QQuickApplicationWindow](qt6/qtdeclarative/src/quicktemplates/qquickapplicationwindow.cpp)
    contentItem()->setFlag(QQuickItem::ItemIsFocusScope);
    contentItem()->setFocus(true);
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

    if (output->objectName() == PRIVATE_WOutputViewport)
        return;

    Q_ASSERT(std::find_if(d->outputs.cbegin(), d->outputs.cend(), [output] (OutputHelper *h) {
                 return h->output() == output;
    }) == d->outputs.cend());

    Q_ASSERT(output->output());

    d->outputs << new OutputHelper(output, this);

    if (d->compositor) {
        auto qwoutput = d->outputs.last()->qwoutput();
        if (qwoutput->handle()->renderer != d->compositor->renderer()->handle())
            qwoutput->initRender(d->compositor->allocator(), d->compositor->renderer());
        Q_EMIT outputViewportInitialized(output);
    }

    if (!d->isInitialized())
        return;

    d->updateSceneDPR();
    d->init(d->outputs.last());
    d->scheduleDoRender();
}

void WOutputRenderWindow::detach(WOutputViewport *output)
{
    if (output->objectName() == PRIVATE_WOutputViewport)
        return;

    Q_D(WOutputRenderWindow);

    int index = d->indexOfOutputHelper(output);
    Q_ASSERT(index >= 0);

    auto outputHelper = d->outputs.takeAt(index);
    outputHelper->invalidate();
    outputHelper->deleteLater();

    if (d->inDestructor || !d->isInitialized())
        return;

    d->updateSceneDPR();
}

void WOutputRenderWindow::attach(WOutputLayer *layer, WOutputViewport *output)
{
    Q_D(WOutputRenderWindow);

    auto wapper = d->ensureOutputLayer(layer);
    Q_ASSERT(!wapper->outputs.contains(output));
    wapper->outputs.append(output);

    auto outputHelper = d->getOutputHelper(output);
    if (outputHelper && outputHelper->attachLayer(wapper))
        d->scheduleDoRender();

    connect(layer, &WOutputLayer::flagsChanged, this, &WOutputRenderWindow::scheduleRender);
    connect(layer, &WOutputLayer::zChanged, this, &WOutputRenderWindow::scheduleRender);
}

void WOutputRenderWindow::detach(WOutputLayer *layer, WOutputViewport *output)
{
    Q_D(WOutputRenderWindow);

    Q_ASSERT(d->indexOfOutputLayer(layer) >= 0);
    auto wapper = d->ensureOutputLayer(layer);
    Q_ASSERT(wapper->outputs.contains(output));
    wapper->outputs.removeOne(output);

    bool ok = layer->disconnect(this);
    Q_ASSERT(ok);

    auto outputHelper = d->getOutputHelper(output);
    if (!outputHelper)
        return;

    outputHelper->detachLayer(wapper);
    d->scheduleDoRender();
}

void WOutputRenderWindow::setOutputScale(WOutputViewport *output, float scale)
{
    Q_D(WOutputRenderWindow);

    if (auto helper = d->getOutputHelper(output)) {
        helper->setScale(scale);
        update();
    }
}

void WOutputRenderWindow::rotateOutput(WOutputViewport *output, WOutput::Transform t)
{
    Q_D(WOutputRenderWindow);

    if (auto helper = d->getOutputHelper(output)) {
        helper->setTransform(t);
        update();
    }
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
        if (qwoutput->handle()->renderer != d->compositor->renderer()->handle())
            qwoutput->initRender(d->compositor->allocator(), d->compositor->renderer());
        Q_EMIT outputViewportInitialized(output->output());
    }

    if (d->componentCompleted && d->compositor->isPolished()) {
        d->init();
    } else {
        connect(newCompositor, &WWaylandCompositor::afterPolish, this, [d] {
            if (d->componentCompleted)
                d->init();
        });
    }
}

WBufferRenderer *WOutputRenderWindow::currentRenderer() const
{
    Q_D(const WOutputRenderWindow);
    return d->rendererList.isEmpty() ? nullptr : d->rendererList.top();
}

void WOutputRenderWindow::render()
{
    Q_D(WOutputRenderWindow);
    d->doRender();
}

void WOutputRenderWindow::render(WOutputViewport *output, bool doCommit)
{
    Q_D(WOutputRenderWindow);
    int index = d->indexOfOutputHelper(output);
    Q_ASSERT(index >= 0);

    d->doRender({d->outputs.at(index)}, true, doCommit);
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

qreal WOutputRenderWindow::width() const
{
    return contentItem()->width();
}

qreal WOutputRenderWindow::height() const
{
    return contentItem()->height();
}

void WOutputRenderWindow::setWidth(qreal arg)
{
    QQuickWindow::setWidth(arg);
    contentItem()->setWidth(arg);
}

void WOutputRenderWindow::setHeight(qreal arg)
{
    QQuickWindow::setHeight(arg);
    contentItem()->setHeight(arg);
}

void WOutputRenderWindow::classBegin()
{
    Q_D(WOutputRenderWindow);
    d->componentCompleted = false;
}

void WOutputRenderWindow::componentComplete()
{
    Q_D(WOutputRenderWindow);
    d->componentCompleted = true;

    if (d->compositor && d->compositor->isPolished())
        d->init();
}

bool WOutputRenderWindow::event(QEvent *event)
{
    Q_D(WOutputRenderWindow);

    if (event->type() == doRenderEventType) {
        QCoreApplication::removePostedEvents(this, doRenderEventType);
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
#include "woutputrenderwindow.moc"
