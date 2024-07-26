// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputrenderwindow.h"
#include "woutput.h"
#include "woutputhelper.h"
#include "wrenderhelper.h"
#include "wbackend.h"
#include "woutputviewport.h"
#include "woutputviewport_p.h"
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
#include <qwoutputlayer.h>
#include <qwegl.h>

#include <QOffscreenSurface>
#include <QQuickRenderControl>
#include <QOpenGLFunctions>
#include <QLoggingCategory>
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
#include <private/qquickitem_p.h>
#include <private/qquickrectangle_p.h>

extern "C" {
#ifdef ENABLE_VULKAN_RENDER
#include <wlr/render/vulkan.h>
#endif
#include <wlr/render/gles2.h>
}

#include <drm_fourcc.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(wlcRenderer, "waylib.server.renderer", QtDebugMsg)
#else
Q_LOGGING_CATEGORY(wlcRenderer, "waylib.server.renderer", QtWarningMsg)
#endif
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

class OutputLayer;
class Q_DECL_HIDDEN OutputHelper : public WOutputHelper
{
    friend class WOutputRenderWindowPrivate;
public:
    struct Q_DECL_HIDDEN LayerData {
        LayerData(OutputLayer *l, qw_output_layer *layer)
            : layer(l)
            , wlrLayer(layer)
            , contentsIsDirty(true)
            , bufferIsUpdatedOnLastRender(false)
        {

        }
        ~LayerData() {
            if (renderer)
                renderer->deleteLater();
            if (wlrLayer)
                wlrLayer->deleteLater();
        }

        OutputLayer *layer;
        qw_output_layer *wlrLayer;
        QPointer<WBufferRenderer> renderer;

        // dirty state
        uint contentsIsDirty:1;
        uint bufferIsUpdatedOnLastRender:1;
        // end

        QRectF mapRect;
        QRectF noClipMapRect;
        QRect mapToOutput;
        QSize pixelSize;
        QMatrix4x4 renderMatrix;

        // for proxy
        QPointer<OutputHelper> mapFrom;
        QPointer<QQuickItem> mapTo;
    };

    OutputHelper(WOutputViewport *output, WOutputRenderWindow *parent, bool renderable, bool contentIsDirty, bool needsFrame)
        : WOutputHelper(output->output(), renderable, contentIsDirty, needsFrame, parent)
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
        // TODO: pre update scale after WOutputHelper::setScale
        output()->output()->safeConnect(&WOutput::scaleChanged, this, &OutputHelper::updateSceneDPR);
    }

    inline qw_output *qwoutput() const {
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

    inline QRectF renderSourceRect() const {
        const auto s = m_output->sourceRect();
        if (s.isValid())
            return s;
        if (m_output->ignoreViewport())
            return {};
        return QRectF(QPointF(0, 0), m_output->size());
    }

    inline QMatrix4x4 renderMatrix() const;
    inline QTransform sourceMapToTargetTransfrom() const {
        return WBufferRenderer::inputMapToOutput(renderSourceRect(),
                                                 m_output->targetRect(),
                                                 m_output->output()->size(),
                                                 devicePixelRatio());
    }
    inline QMatrix4x4 mapToViewport(QQuickItem *source) const;

    void updateSceneDPR();

    int indexOfLayer(OutputLayer *layer) const;
    bool attachLayer(OutputLayer *layer);
    void detachLayer(OutputLayer *layer);
    void sortLayers();
    void cleanLayerCompositor();

    inline qw_buffer *beginRender(WBufferRenderer *renderer,
                                 const QSize &pixelSize, uint32_t format,
                                 WBufferRenderer::RenderFlags flags);
    inline void render(WBufferRenderer *renderer, int sourceIndex, const QMatrix4x4 &renderMatrix,
                       const QRectF &sourceRect, const QRectF &viewportRect, bool preserveColorContents);

    static bool visualizeLayers() {
        static bool on = qEnvironmentVariableIsSet("WAYLIB_VISUALIZE_LAYERS");
        return on;
    }

    qw_buffer *renderLayer(LayerData *layer, bool *dontEndRenderAndReturnNeedsEndRender,
                          bool isCursor,
                          const QSize &forcePixelSize = QSize());
    WBufferRenderer *afterRender();
    WBufferRenderer *compositeLayers(const QVector<LayerData*> layers, bool forceShadowRenderer);
    bool commit(WBufferRenderer *buffer);
    bool tryToHardwareCursor(LayerData *layer, wlr_buffer *buffer);

private:
    WOutputViewport *m_output = nullptr;
    QList<LayerData*> m_layers;
    WBufferRenderer *m_lastCommitBuffer = nullptr;
    // only for render cursor
    std::unique_ptr<LayerData> m_cursorLayer;

    // for compositeLayers
    QPointer<WOutputViewport> m_output2;
    QPointer<QQuickItem> m_layerPorxyContainer;
    QList<QPointer<WQuickTextureProxy>> m_layerProxys;
};

class Q_DECL_HIDDEN OutputLayer
{
    friend class OutputHelper;
public:
    OutputLayer(WOutputLayer *layer)
        : layer(layer) {}

    inline QList<WOutputViewport*> &outputs() {
        return m_outputs;
    }
    inline void beforeRender(WOutputRenderWindow *window) {
        setEnabled(!window->disableLayers() || layer->force());
        state = Normal;
    }
    inline void setEnabled(bool enable) {
        if (enabled == enable)
            return;
        enabled = enable;
        layer->setAccepted(enabled);
    }
    inline bool isEnabled() const {
        return enabled && layer->parent()->isVisible();
    }
    inline bool tryAccept() const {
        return state != Rejected;
    }
    inline bool accept(WOutputViewport *output, bool isHardware) {
        if (state == Rejected)
            return false;
        state = Accepted;
        if (!layer->isAccepted()) {
            layer->setAccepted(true);
            qCInfo(wlcRenderer) << "Layer" << layer->parent() << "is accepted("
                                << (isHardware ? "hardware" : "software") << ") on"
                                << output;
        }

        if (layer->setInHardware(output, isHardware)) {
            WOutputViewportPrivate::get(output)->notifyHardwareLayersChanged();
        }

        return true;
    }
    inline bool tryReject() const {
        return state != Accepted;
    }
    inline bool reject(WOutputViewport *output) {
        if (state == Accepted)
            return false;
        state = Rejected;
        if (layer->isAccepted()) {
            layer->setAccepted(false);
            qCInfo(wlcRenderer) << "Layer" << layer->parent() << "is rejected on" << output;
        }

        if (layer->setInHardware(output, false)) {
            WOutputViewportPrivate::get(output)->notifyHardwareLayersChanged();
        }

        return true;
    }
    inline void reset() {
        layer->setAccepted(true);
    }
    inline bool needsComposite() const {
        if (state != Normal)
            Q_ASSERT(layer->isAccepted() == (state == Accepted));
        if (!enabled)
            Q_ASSERT(layer->isAccepted());
        return layer->isAccepted();
    }
    inline bool forceLayer() const {
        return layer->force();
    }
    inline bool keepLayer() const {
        return layer->keepLayer();
    }

private:
    friend class WOutputRenderWindow;
    friend class WOutputRenderWindowPrivate;
    WOutputLayer *layer;
    bool enabled = true;

    enum State {
        Normal,
        Accepted,
        Rejected,
    };

    State state;

    QList<WOutputViewport*> m_outputs;
};

class Q_DECL_HIDDEN RenderControl : public QQuickRenderControl
{
public:
    RenderControl() = default;

    QWindow *renderWindow(QPoint *) override {
        return m_renderWindow;
    }

    QWindow *m_renderWindow = nullptr;
};

static QEvent::Type doRenderEventType = static_cast<QEvent::Type>(QEvent::registerEventType());
class Q_DECL_HIDDEN WOutputRenderWindowPrivate : public QQuickWindowPrivate
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

    int indexOfOutputHelper(const WOutputViewport *output) const;
    inline OutputHelper *getOutputHelper(const WOutputViewport *output) const {
        int index = indexOfOutputHelper(output);
        if (index >= 0)
            return outputs.at(index);
        return nullptr;
    }

    int indexOfOutputLayer(const WOutputLayer *layer) const;
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
    void sortOutputs();

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

    Q_DECLARE_PUBLIC(WOutputRenderWindow)

    bool componentCompleted = true;
    bool inRendering = false;

    QPointer<qw_renderer> m_renderer;
    QPointer<qw_allocator> m_allocator;

    QList<OutputHelper*> outputs;
    QList<OutputLayer*> layers;
    bool disableLayers = false;

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

QMatrix4x4 OutputHelper::renderMatrix() const
{
    QMatrix4x4 renderMatrix;

    if (auto customTransform = output()->viewportTransform()) {
        customTransform->applyTo(&renderMatrix);
    } else if (!output()->ignoreViewport() && output()->input() != output()) {
        auto viewportMatrix = QQuickItemPrivate::get(output())->itemNode()->matrix().inverted();
        if (auto inputItem = output()->input()) {
            QMatrix4x4 matrix = QQuickItemPrivate::get(output()->parentItem())->itemToWindowTransform();
            matrix *= QQuickItemPrivate::get(inputItem)->windowToItemTransform();
            renderMatrix = viewportMatrix * matrix.inverted();
        } else { // the input item is window's contentItem
            QMatrix4x4 parentMatrix = QQuickItemPrivate::get(output()->parentItem())->itemToWindowTransform().inverted();
            renderMatrix = viewportMatrix * parentMatrix;
        }
    }

    return renderMatrix;
}

QMatrix4x4 OutputHelper::mapToViewport(QQuickItem *source) const
{
    if (source == m_output->input())
        return renderMatrix() * sourceMapToTargetTransfrom();
    auto sd = QQuickItemPrivate::get(source);
    auto matrix = sd->itemToWindowTransform();

    if (m_output->input()) {
        matrix *= QQuickItemPrivate::get(m_output->input())->windowToItemTransform();
        return renderMatrix() * matrix * sourceMapToTargetTransfrom();
    } else { // the input item is window's contentItem
        matrix *= QQuickItemPrivate::get(m_output)->windowToItemTransform();
        return matrix * sourceMapToTargetTransfrom();
    }
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
    auto qwlayer = qw_output_layer::create(*qwoutput());
    if (!qwlayer)
        return false;
    qwlayer->setParent(this);

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

    if (m_cursorLayer && m_cursorLayer->layer == layer) {
        // Clear hardware cursor
        tryToHardwareCursor(nullptr, nullptr);
        m_cursorLayer.reset();
    }
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

    for (auto proxy : std::as_const(tmpList)) {
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

qw_buffer *OutputHelper::beginRender(WBufferRenderer *renderer,
                                    const QSize &pixelSize, uint32_t format,
                                    WBufferRenderer::RenderFlags flags)
{
    return renderer->beginRender(pixelSize, devicePixelRatio(), format, flags);
}

void OutputHelper::render(WBufferRenderer *renderer, int sourceIndex, const QMatrix4x4 &renderMatrix,
                          const QRectF &sourceRect, const QRectF &targetRect, bool preserveColorContents)
{
    renderWindowD()->pushRenderer(renderer);
    renderer->render(sourceIndex, renderMatrix, sourceRect, targetRect, preserveColorContents);
}

static QQuickItem *createVisualRectangle(QQuickItem *target, const QColor &color) {
    auto rectangle = new QQuickRectangle(target);
    rectangle->border()->setColor(color);
    rectangle->border()->setWidth(1);
    rectangle->setColor(Qt::transparent);
    QQuickItemPrivate::get(rectangle)->anchors()->setFill(target);

    return rectangle;
}

qw_buffer *OutputHelper::renderLayer(LayerData *layer, bool *dontEndRenderAndReturnNeedsEndRender,
                                    bool isCursor, const QSize &forcePixelSize)
{
    auto source = layer->layer->layer->parent();
    if (!layer->renderer) {
        layer->renderer = new WBufferRenderer(source);

        QList<QQuickItem*> sourceList {source};
        if (visualizeLayers()) {
            auto rectangle = createVisualRectangle(source, Qt::green);
            QQuickItemPrivate::get(rectangle)->refFromEffectItem(true);
            // Ensure QSGRootNode for this item
            renderWindowD()->updateDirtyNode(rectangle);
            sourceList << rectangle;
        }

        layer->renderer->setSourceList(sourceList, false);
        layer->renderer->setOutput(output()->output());

        connect(layer->renderer, &WBufferRenderer::sceneGraphChanged, this, [layer] {
            layer->contentsIsDirty = true;
        });
    }

    layer->bufferIsUpdatedOnLastRender = false;
    layer->mapRect = QRectF(QPointF(0, 0), source->size());
    const auto mapFrom = layer->mapFrom ? layer->mapFrom.get() : this;
    QMatrix4x4 mapMatrix = mapFrom->mapToViewport(source);
    // map to WOutputViewport
    layer->mapRect = mapMatrix.mapRect(layer->mapRect);
    layer->noClipMapRect = layer->mapRect;
    // clip to WOutputViewport
    layer->mapRect = layer->mapRect & QRectF(QPointF(0, 0), mapFrom->output()->size());

    if (layer->mapTo) {
        const auto tmpMatrix = mapToViewport(layer->mapTo);
        layer->mapRect = tmpMatrix.mapRect(layer->mapRect);
        layer->noClipMapRect = tmpMatrix.mapRect(layer->noClipMapRect);
        mapMatrix = tmpMatrix * mapMatrix;
    }

    if (!layer->mapFrom // If is a copy layer form the other viewport, should always follow transformation
        && !layer->layer->layer->flags().testFlag(WOutputLayer::SizeFollowTransformation)) {
        QSizeF size = source->size();

        if (layer->layer->layer->flags().testFlag(WOutputLayer::SizeFollowItemTransformation)) {
            const auto sourceD = QQuickItemPrivate::get(source);
            size = sourceD->itemNode()->matrix().mapRect(QRectF(QPointF(0, 0), size)).size();
        }

        if (output()->output()->orientation() % 2 != 0) {
            std::swap(size.rwidth(), size.rheight());
        }

        layer->mapRect.setSize(size);
    }

    const qreal dpr = devicePixelRatio();
    QSize pixelSize = forcePixelSize;
    if (!pixelSize.isValid()) {
        const auto tmpSize = layer->mapRect.size() * dpr;
        pixelSize.rwidth() = qCeil(tmpSize.width());
        pixelSize.rheight() = qCeil(tmpSize.height());
    }

    layer->mapToOutput = QRect((layer->mapRect.topLeft() * dpr).toPoint(), pixelSize);
    if (layer->mapRect.isEmpty()) {
        return nullptr;
    }
    Q_ASSERT(!pixelSize.isEmpty());

    auto mapToViewportMatrixData = mapMatrix.constData();
    const QMatrix4x4 renderMatrix {
        mapToViewportMatrixData[0],
        mapToViewportMatrixData[1 * 4 + 0],
        mapToViewportMatrixData[2 * 4 + 0],
        mapToViewportMatrixData[3 * 4 + 0] - static_cast<float>(layer->mapRect.x()),
        mapToViewportMatrixData[1],
        mapToViewportMatrixData[1 * 4 + 1],
        mapToViewportMatrixData[2 * 4 + 1],
        mapToViewportMatrixData[3 * 4 + 1] - static_cast<float>(layer->mapRect.y()),
        mapToViewportMatrixData[2],
        mapToViewportMatrixData[1 * 4 + 2],
        mapToViewportMatrixData[2 * 4 + 2],
        mapToViewportMatrixData[3 * 4 + 2],
        mapToViewportMatrixData[3],
        mapToViewportMatrixData[1 * 4 + 3],
        mapToViewportMatrixData[2 * 4 + 3],
        mapToViewportMatrixData[3 * 4 + 3],
    };

    auto buffer = layer->renderer->lastBuffer();
    const bool needsFullUpdate = layer->pixelSize != pixelSize
                                 || layer->renderMatrix != renderMatrix;

    if (!buffer || layer->contentsIsDirty || needsFullUpdate) {
        layer->renderer->setSize(pixelSize / dpr);

        const bool alpha = !layer->layer->layer->flags().testFlag(WOutputLayer::NoAlpha);
        buffer = beginRender(layer->renderer, pixelSize,
                             // TODO: Allows control format by WOutputLayer
                             alpha ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888,
                             isCursor ? WBufferRenderer::UseCursorFormats
                                      : WBufferRenderer::DontConfigureSwapchain);
        if (buffer) {
            const QRectF sr = QRectF(layer->mapRect.topLeft() - layer->noClipMapRect.topLeft(), layer->mapRect.size());
            const QRectF tr(QPointF(0, 0), layer->mapRect.size());
            render(layer->renderer, 0, renderMatrix, sr, tr,
                   layer->layer->layer->flags().testFlag(WOutputLayer::PreserveColorContents));

            if (visualizeLayers())
                render(layer->renderer, 1, renderMatrix, sr, tr, true);

            if (dontEndRenderAndReturnNeedsEndRender) {
                *dontEndRenderAndReturnNeedsEndRender = true;
            } else {
                layer->renderer->endRender();
            }

            layer->bufferIsUpdatedOnLastRender = true;
        } else if (dontEndRenderAndReturnNeedsEndRender) {
            *dontEndRenderAndReturnNeedsEndRender = false;
        }
    }

    layer->contentsIsDirty = false;
    layer->renderMatrix = renderMatrix;
    layer->pixelSize = pixelSize;

    return buffer;
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
    if (m_layers.isEmpty()) {
        cleanLayerCompositor();
        return bufferRenderer();
    }

    // update layers
    wlr_output_layer_state_array layers;
    QList<LayerData*> needsCompositeLayers;
    layers.reserve(m_layers.size());
    needsCompositeLayers.reserve(m_layers.size());
    int firstCantRejectLayerIndex = m_layers.size();

    for (LayerData *i : std::as_const(m_layers)) {
        if (!i->layer->isEnabled())
            continue;

        // TODO: Should continue if the render format is changed
        if (!i->layer->needsComposite())
            continue;

        bool needsEndBuffer = false;
        auto buffer = renderLayer(i, &needsEndBuffer, false);
        if (!buffer)
            continue;

        layers.append({
            .layer = i->wlrLayer->handle(),
            .buffer = buffer->handle(),
            .dst_box = {
                .x = i->mapToOutput.x(),
                .y = i->mapToOutput.y(),
                .width = i->mapToOutput.width(),
                .height = i->mapToOutput.height(),
            },
            .damage = &i->renderer->damageRing()->handle()->current
        });

        if (needsEndBuffer) {
            // after get damage(&i->renderer->damageRing()->handle()->current)
            i->renderer->endRender();
        }

        Q_ASSERT(!i->renderer->currentBuffer());
        needsCompositeLayers.append(i);

        if (firstCantRejectLayerIndex == m_layers.size() && !i->layer->tryReject())
            firstCantRejectLayerIndex = needsCompositeLayers.size() - 1;
    }

    if (layers.isEmpty()) {
        cleanLayerCompositor();
        return bufferRenderer();
    }

    if (output()->offscreen()) {
        cleanLayerCompositor();
        return bufferRenderer();
    }

    const bool ok = WOutputHelper::testCommit(bufferRenderer()->currentBuffer(), layers);
    int needsSoftwareCompositeBeginIndex = -1;
    int needsSoftwareCompositeEndIndex = -1;
    bool forceShadowRender = false;
    bool hasHardwareCursor = false;
    bool hasCursorLayer = false;

    for (int i = layers.length() - 1; i >= 0; --i) {
        const auto &state = layers.at(i);
        Q_ASSERT(state.buffer);
        OutputLayer *layer = needsCompositeLayers[i]->layer;
        if (layer->layer->flags().testFlag(WOutputLayer::Cursor))
            hasCursorLayer = true;

        // If hardware layers is disabled on this output viewport
        // and this layer doesn't want force layer, should fallback
        // to software composite.
        if (needsSoftwareCompositeEndIndex == -1
            && output()->disableHardwareLayers()
            && !layer->forceLayer()) {
            needsSoftwareCompositeEndIndex = i;
        }

        if (needsSoftwareCompositeEndIndex == -1) {
            if (ok && state.accepted) {
                bool ok = layer->accept(output(), true);
                Q_ASSERT(ok);
                continue;
            } else {
                // try fallback to cursor plane
                Q_ASSERT(needsSoftwareCompositeEndIndex == -1);
                if (!hasHardwareCursor
                    && tryToHardwareCursor(needsCompositeLayers[i], layers.at(i).buffer)) {
                    hasHardwareCursor = true;
                    bool ok = layer->accept(output(), true);
                    Q_ASSERT(ok);
                    needsSoftwareCompositeEndIndex = i - 1;
                    continue;
                } else {
                    needsSoftwareCompositeEndIndex = i;
                }
            }
        }

        if (layer->forceLayer() || layer->keepLayer()
            // current layer can't reject, because the layes behind current layer
            // requset force composite.
            || i >= firstCantRejectLayerIndex) {
            Q_ASSERT(layer->needsComposite());
            bool ok = layer->accept(output(), false);
            Q_ASSERT(ok);
            if (layer->forceLayer())
                forceShadowRender = true;
            needsSoftwareCompositeBeginIndex = i;
        } else if (!output()->ignoreSoftwareLayers()) {
            bool ok = layer->reject(output());
            Q_ASSERT(ok);
        }
    }

    if (!hasHardwareCursor && m_cursorLayer) {
        // Clear hardware cursor
        tryToHardwareCursor(nullptr, nullptr);
        // Don't reset m_cursorLayer, maybe will use in next frame
    }

    if (!hasCursorLayer) {
        Q_ASSERT(!hasHardwareCursor);
        m_cursorLayer.reset();
    }

    if (needsSoftwareCompositeEndIndex == -1) // All layers need software composite
        needsSoftwareCompositeEndIndex = needsCompositeLayers.size() - 1;
    const int needsCompositeCount = needsSoftwareCompositeBeginIndex >= 0
                                        ? needsSoftwareCompositeEndIndex - needsSoftwareCompositeBeginIndex + 1
                                        : 0;
    if (needsCompositeCount > 0) {
        Q_ASSERT(layers.size() == needsCompositeLayers.size());
        // Don't use needsCompositeCount, rejected layers also should remove
        layers.remove(0, needsSoftwareCompositeEndIndex + 1);

        needsCompositeLayers = needsCompositeLayers.mid(needsSoftwareCompositeBeginIndex,
                                                        needsCompositeCount);
    } else {
        needsCompositeLayers.clear();
    }

    setLayers(layers);

    if (needsCompositeLayers.isEmpty()
        // Don't do anyting if this output viewport wants ignore software layers
        || output()->ignoreSoftwareLayers()) {
        Q_ASSERT(!forceShadowRender);
        cleanLayerCompositor();
        return bufferRenderer();
    }

    return compositeLayers(needsCompositeLayers, forceShadowRender);
}

#define PRIVATE_WOutputViewport "__private_WOutputViewport"
WBufferRenderer *OutputHelper::compositeLayers(const QList<LayerData*> layers, bool forceShadowRenderer)
{
    Q_ASSERT(!layers.isEmpty());

    const bool usingShadowRenderer = forceShadowRenderer
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

    for (int i = 0; i < layers.count(); ++i) {
        const int j = i + (usingShadowRenderer ? 1 : 0);
        WQuickTextureProxy *proxy = nullptr;
        if (j < m_layerProxys.size()) {
            proxy = m_layerProxys.at(j);
        } else {
            proxy = new WQuickTextureProxy(m_layerPorxyContainer);
            if (visualizeLayers())
                createVisualRectangle(proxy, Qt::red);
            m_layerProxys.append(proxy);
        }

        LayerData *layer = layers.at(i);
        layer->renderer->setForceCacheBuffer(true);
        proxy->setSourceItem(layer->renderer);
        proxy->setPosition(layer->mapRect.topLeft());
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
            // stop primary render
            if (bufferRenderer()->currentBuffer())
                bufferRenderer()->endRender();
            render(bufferRenderer2(), 0, {}, renderSourceRect(), m_output->targetRect(), true);

            return bufferRenderer2();
        }
    } else {
        if (bufferRenderer()->currentBuffer()) {
            render(bufferRenderer(), 1, {}, renderSourceRect(), m_output->targetRect(), true);
        } else {
            // ###(zccrs): Maybe because contents is not dirty, so not do render
            // in WOutputRenderWindowPrivate::doRenderOutputs, force mark the
            // contents to dirty here to ensure can render layers in the next frame.
            update();
        }
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

bool OutputHelper::tryToHardwareCursor(LayerData *layer, wlr_buffer *buffer)
{
    do {
        auto set_cursor = qwoutput()->handle()->impl->set_cursor;
        if (!buffer) {
            if (set_cursor)
                set_cursor(qwoutput()->handle(), buffer, 0, 0);
            return true;
        }

        if (!layer->layer->layer->flags().testFlag(WOutputLayer::Cursor))
            break;

        if (qwoutput()->handle()->software_cursor_locks > 0)
            break;

        auto move_cursor = qwoutput()->handle()->impl->move_cursor;

        if (!set_cursor || !move_cursor)
            break;

        QSize pixelSize = QSize(buffer->width, buffer->height);
        auto get_cursor_size = qwoutput()->handle()->impl->get_cursor_size;
        auto get_cursor_formsts = qwoutput()->handle()->impl->get_cursor_formats;
        if (get_cursor_size) {
            get_cursor_size(qwoutput()->handle(), &pixelSize.rwidth(), &pixelSize.rheight());
        }

        if (pixelSize != QSize(buffer->width, buffer->height)
            || get_cursor_formsts) {
            // needs render cursor again
            if (!m_cursorLayer || m_cursorLayer->layer != layer->layer) {
                // TODO: Direct rendr the buffer to a new wlr_buffer, maybe can use wlr_render_pass
                m_cursorLayer.reset(new LayerData(layer->layer, nullptr));
                m_cursorLayer->mapFrom = layer->mapFrom;
                m_cursorLayer->mapTo = layer->mapTo;
            }

            m_cursorLayer->contentsIsDirty = layer->bufferIsUpdatedOnLastRender;

            auto newBuffer = renderLayer(m_cursorLayer.get(), nullptr, true, pixelSize);
            if (!newBuffer)
                break;
            Q_ASSERT(pixelSize.width() == newBuffer->handle()->width);
            Q_ASSERT(pixelSize.height() == newBuffer->handle()->height);
            buffer = newBuffer->handle();
        }

        const auto hotSpot = layer->renderMatrix.map(layer->layer->layer->cursorHotSpot()
                                                     * devicePixelRatio()).toPoint();
        if (!set_cursor(qwoutput()->handle(), buffer, hotSpot.x(), hotSpot.y())) {
            break;
        } else {
            resetGlState();
        }

        const auto pos = layer->mapToOutput.topLeft() + hotSpot;
        wlr_box cleanTransform {.x = pos.x(), .y = pos.y()};
        const auto outputSize = output()->output()->size();
        // the layer->mapRect has been transform in renderLayer, but
        // wlroot's move_cursor also will transform the cursor's position.
        // so revert transform here.
        wlr_box_transform(&cleanTransform, &cleanTransform,
                          qwoutput()->handle()->transform,
                          outputSize.width(), outputSize.height());
        if (!move_cursor(qwoutput()->handle(), cleanTransform.x, cleanTransform.y)) {
            break;
        }

        return true;
    } while (false);

    resetGlState();

    return false;
}

int WOutputRenderWindowPrivate::indexOfOutputHelper(const WOutputViewport *output) const
{
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i)->output() == output) {
            return i;
        }
    }

    return -1;
}

int WOutputRenderWindowPrivate::indexOfOutputLayer(const WOutputLayer *layer) const
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
    Q_ASSERT(m_renderer);
    Q_Q(WOutputRenderWindow);

    if (QSGRendererInterface::isApiRhiBased(graphicsApi()))
        initRCWithRhi();
    Q_ASSERT(context);
    q->create();
    rc()->m_renderWindow = q;

    for (auto output : std::as_const(outputs))
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
                     q, qOverload<>(&WOutputRenderWindow::update));
    QObject::connect(rc(), &QQuickRenderControl::sceneChanged,
                     q, qOverload<>(&WOutputRenderWindow::update));

    Q_EMIT q->initialized();
}

void WOutputRenderWindowPrivate::init(OutputHelper *helper)
{
    W_Q(WOutputRenderWindow);
    QMetaObject::invokeMethod(q, &WOutputRenderWindow::scheduleRender, Qt::QueuedConnection);
    helper->init();
    QObject::connect(helper->output(), &WOutputViewport::dependsChanged, helper, [this] {
        sortOutputs();
    });

    for (auto layer : std::as_const(layers)) {
        if (layer->outputs().contains(helper->output())) {
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

        auto phdev = wlr_vk_renderer_get_physical_device(m_renderer->handle());
        auto dev = wlr_vk_renderer_get_device(m_renderer->handle());
        auto queue_family = wlr_vk_renderer_get_queue_family(m_renderer->handle());

#if QT_VERSION > QT_VERSION_CHECK(6, 6, 0)
        auto instance = wlr_vk_renderer_get_instance(m_renderer->handle());
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
        Q_ASSERT(wlr_renderer_is_gles2(m_renderer->handle()));
        auto egl = wlr_gles2_renderer_get_egl(m_renderer->handle());
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
    if (outputs.isEmpty()
        // Maybe the platform window is destroyed
        || !platformWindow) {
        return;
    }

    qreal maxDPR = 0.0;

    for (auto o : std::as_const(outputs)) {
        if (o->output()->output()->scale() > maxDPR)
            maxDPR = o->output()->output()->scale();
    }

    setSceneDevicePixelRatio(maxDPR);
}

void WOutputRenderWindowPrivate::sortOutputs()
{
    std::stable_sort(outputs.begin(), outputs.end(),
                     [] (const OutputHelper *o1, const OutputHelper *o2) {
        return o2->output()->depends().contains(o1->output());
    });
}

QVector<std::pair<OutputHelper*, WBufferRenderer*>>
WOutputRenderWindowPrivate::doRenderOutputs(const QList<OutputHelper*> &outputs, bool forceRender)
{
    QVector<OutputHelper*> renderResults;
    renderResults.reserve(outputs.size());
    for (OutputHelper *helper : std::as_const(outputs)) {
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
        const auto renderMatrix = helper->renderMatrix();

        // maybe using the other WOutputViewport's QSGTextureProvider
        if (!helper->output()->depends().isEmpty())
            renderControl->sync();

        qw_buffer *buffer = helper->beginRender(helper->bufferRenderer(), helper->output()->output()->size(), format,
                                                WBufferRenderer::RedirectOpenGLContextDefaultFrameBufferObject);
        Q_ASSERT(buffer == helper->bufferRenderer()->currentBuffer());
        if (buffer) {
            helper->render(helper->bufferRenderer(), 0, renderMatrix,
                           helper->renderSourceRect(), helper->output()->targetRect(),
                           helper->output()->preserveColorContents());
        }
        renderResults.append(helper);
    }

    QVector<std::pair<OutputHelper*, WBufferRenderer*>> needsCommit;
    needsCommit.reserve(renderResults.size());
    for (auto helper : std::as_const(renderResults)) {
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

    W_Q(WOutputRenderWindow);
    for (OutputLayer *layer : std::as_const(layers)) {
        layer->beforeRender(q);
    }

    rc()->polishItems();
    if (QSGRendererInterface::isApiRhiBased(WRenderHelper::getGraphicsApi()))
        rc()->beginFrame();
    rc()->sync();

    QQuickAnimatorController_advance(animationController.get());
    Q_EMIT q->beforeRendering();
    runAndClearJobs(&beforeRenderingJobs);

    auto needsCommit = doRenderOutputs(outputs, forceRender);

    Q_EMIT q->afterRendering();
    runAndClearJobs(&afterRenderingJobs);

    if (QSGRendererInterface::isApiRhiBased(WRenderHelper::getGraphicsApi()))
        rc()->endFrame();

    if (doCommit) {
        for (auto i : std::as_const(needsCommit)) {
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

    Q_ASSERT(output->output());
    bool initialRenderable = false;
    bool initialContentIsDirty = false;
    bool initialNeedsFrame = false;
    for (const auto &helper : std::as_const(d->outputs)) {
        Q_ASSERT(helper->output() != output);
        if (helper->qwoutput() == output->output()->handle()) {
            // For a new viewport, it should initialize state from viewports with the same output.
            initialRenderable |= helper->renderable();
            initialContentIsDirty |= helper->contentIsDirty();
            initialNeedsFrame |= helper->needsFrame();
        }
    }
    auto newOutput = new OutputHelper(output,
                                      this,
                                      initialRenderable,
                                      initialContentIsDirty,
                                      initialNeedsFrame);
    d->outputs << newOutput;
    d->sortOutputs();

    if (d->m_renderer) {
        auto qwoutput = newOutput->qwoutput();
        if (qwoutput->handle()->renderer != d->m_renderer->handle())
            qwoutput->init_render(d->m_allocator->handle(), d->m_renderer->handle());
        Q_EMIT outputViewportInitialized(output);
    }

    if (!d->isInitialized())
        return;

    d->updateSceneDPR();
    d->init(newOutput);
    d->scheduleDoRender();

    if (!newOutput->layers().isEmpty()) {
        if (auto od = WOutputViewportPrivate::get(output)) {
            od->notifyLayersChanged();
            od->notifyHardwareLayersChanged();
        }
    }
}

void WOutputRenderWindow::detach(WOutputViewport *output)
{
    if (output->objectName() == PRIVATE_WOutputViewport)
        return;

    Q_D(WOutputRenderWindow);

    int index = d->indexOfOutputHelper(output);
    Q_ASSERT(index >= 0);

    auto outputHelper = d->outputs.takeAt(index);
    const auto hasLayer = !outputHelper->layers().isEmpty();
    outputHelper->invalidate();
    outputHelper->deleteLater();

    if (d->inDestructor || !d->isInitialized())
        return;

    d->updateSceneDPR();

    if (hasLayer) {
        if (auto od = WOutputViewportPrivate::get(output)) {
            od->notifyLayersChanged();
            od->notifyHardwareLayersChanged();
        }
    }
}

void WOutputRenderWindow::attach(WOutputLayer *layer, WOutputViewport *output)
{
    Q_D(WOutputRenderWindow);

    auto wapper = d->ensureOutputLayer(layer);
    Q_ASSERT(!wapper->outputs().contains(output));
    wapper->outputs().append(output);

    auto outputHelper = d->getOutputHelper(output);
    if (outputHelper && outputHelper->attachLayer(wapper))
        d->scheduleDoRender();

    connect(layer, &WOutputLayer::flagsChanged, this, &WOutputRenderWindow::scheduleRender);
    connect(layer, &WOutputLayer::zChanged, this, &WOutputRenderWindow::scheduleRender);

    if (auto od = WOutputViewportPrivate::get(output)) {
        od->notifyLayersChanged();
        od->notifyHardwareLayersChanged();
    }
}

void WOutputRenderWindow::attach(WOutputLayer *layer, WOutputViewport *output,
                                 WOutputViewport *mapFrom, QQuickItem *mapTo)
{
    attach(layer, output);
    Q_D(WOutputRenderWindow);

    auto index = d->indexOfOutputLayer(layer);
    Q_ASSERT(index >= 0);
    OutputLayer *wapper = d->layers.at(index);

    index = d->indexOfOutputHelper(output);
    Q_ASSERT(index >= 0);
    OutputHelper *helper = d->outputs.at(index);

    index = helper->indexOfLayer(wapper);
    Q_ASSERT(index >= 0);
    OutputHelper::LayerData *layerData = helper->layers().at(index);

    index = d->indexOfOutputHelper(mapFrom);
    Q_ASSERT(index >= 0);
    layerData->mapFrom = d->outputs.at(index);
    layerData->mapTo = mapTo;
}

void WOutputRenderWindow::detach(WOutputLayer *layer, WOutputViewport *output)
{
    Q_D(WOutputRenderWindow);

    Q_ASSERT(d->indexOfOutputLayer(layer) >= 0);
    auto wapper = d->ensureOutputLayer(layer);
    Q_ASSERT(wapper->outputs().contains(output));
    wapper->outputs().removeOne(output);

    bool ok = layer->disconnect(this);
    Q_ASSERT(ok);

    auto outputHelper = d->getOutputHelper(output);
    if (!outputHelper)
        return;

    outputHelper->detachLayer(wapper);
    d->scheduleDoRender();

    if (auto od = WOutputViewportPrivate::get(output)) {
        od->notifyLayersChanged();
        od->notifyHardwareLayersChanged();
    }
}

QList<WOutputLayer *> WOutputRenderWindow::layers(const WOutputViewport *output) const
{
    Q_D(const WOutputRenderWindow);

    QList<WOutputLayer*> list;

    int index = d->indexOfOutputHelper(output);
    Q_ASSERT(index >= 0);
    OutputHelper *helper = d->outputs.at(index);

    for (auto l : std::as_const(helper->layers()))
        list << l->layer->layer;

    return list;
}

QList<WOutputLayer *> WOutputRenderWindow::hardwareLayers(const WOutputViewport *output) const
{
    Q_D(const WOutputRenderWindow);

    QList<WOutputLayer*> list;

    int index = d->indexOfOutputHelper(output);
    Q_ASSERT(index >= 0);
    OutputHelper *helper = d->outputs.at(index);

    for (auto l : std::as_const(helper->layers()))
        if (l->layer->layer->inOutputsByHardware().contains(output))
            list << l->layer->layer;

    return list;
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

void WOutputRenderWindow::init(qw_renderer *renderer, qw_allocator *allocator)
{
    Q_D(WOutputRenderWindow);
    Q_ASSERT(!d->m_renderer);
    Q_ASSERT(!d->m_allocator);
    Q_ASSERT(renderer);
    Q_ASSERT(allocator);

    d->m_renderer = renderer;
    d->m_allocator = allocator;

    for (auto output : std::as_const(d->outputs)) {
        auto qwoutput = output->qwoutput();
        if (qwoutput->handle()->renderer != d->m_renderer->handle())
            qwoutput->init_render(d->m_allocator->handle(), d->m_renderer->handle());
        Q_EMIT outputViewportInitialized(output->output());
    }

    if (d->componentCompleted) {
        d->init();
    }
}

qw_renderer *WOutputRenderWindow::renderer() const
{
    Q_D(const WOutputRenderWindow);
    return d->m_renderer;
}

qw_allocator *WOutputRenderWindow::allocator() const
{
    Q_D(const WOutputRenderWindow);
    return d->m_allocator;
}

WBufferRenderer *WOutputRenderWindow::currentRenderer() const
{
    Q_D(const WOutputRenderWindow);
    return d->rendererList.isEmpty() ? nullptr : d->rendererList.top();
}

QList<QPointer<QQuickItem>> WOutputRenderWindow::paintOrderItemList(QQuickItem *root, std::function<bool(QQuickItem*)> filter)
{
    QStack<QQuickItem *> nodes;
    QList<QPointer<QQuickItem>> result;
    nodes.push(root);
    while (!nodes.isEmpty()){
        auto node = nodes.pop();
        if (filter(node)) {
            result.append(node);
        }
        auto childItems = QQuickItemPrivate::get(node)->paintOrderChildItems();
        for (auto child = childItems.crbegin(); child != childItems.crend(); child++) {
            nodes.push(*child);
        }
    }
    return result;
}

bool WOutputRenderWindow::disableLayers() const
{
    Q_D(const WOutputRenderWindow);
    return d->disableLayers;
}

void WOutputRenderWindow::setDisableLayers(bool newDisableLayers)
{
    Q_D(WOutputRenderWindow);
    if (d->disableLayers == newDisableLayers)
        return;
    d->disableLayers = newDisableLayers;
    d->scheduleDoRender();
    Q_EMIT disableLayersChanged();
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
    for (auto o : std::as_const(d->outputs))
        o->update(); // make contents to dirty
    d->scheduleDoRender();
}

void WOutputRenderWindow::update(WOutputViewport *output)
{
    Q_D(WOutputRenderWindow);
    int index = d->indexOfOutputHelper(output);
    Q_ASSERT(index >= 0);
    d->outputs.at(index)->update();
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

    if (d->m_renderer)
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
