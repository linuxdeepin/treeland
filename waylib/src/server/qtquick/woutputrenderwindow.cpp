// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
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
#include "wpresentation.h"
#include "wbufferrenderer_p.h"
#include "wquicktextureproxy.h"
#include "wquickcursor.h"
#include "wsgtextureprovider.h"
#include "wsurface.h"
#include "weventjunkman.h"
#include "winputdevice.h"
#include "wseat.h"
#include "wayliblogging.h"
#include "utils/private/wvulkantrace_p.h"

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
#include <qwoutputinterface.h>
#ifdef ENABLE_VULKAN_RENDER
#include <qwvulkan.h>
#endif

#include <QOffscreenSurface>
#include <QElapsedTimer>
#include <QQuickRenderControl>
#include <QOpenGLFunctions>
#include <QRunnable>
#include <QSet>
#include <memory>
#include <cstring>

#include <private/qsgrenderer_p.h>
#include <private/qsgsoftwarerenderer_p.h>
#include <private/qquickanimatorcontroller_p.h>
#include "private/wprivateaccessor_p.h"
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
#include <wlr/render/gles2.h>
}

#include <drm_fourcc.h>
#include <limits>

using QQuickAnimCtrl_AnimRoots_t = QHash<QAbstractAnimationJob *, QSharedPointer<QAbstractAnimationJob>>;
W_DECLARE_PRIVATE_MEMBER(QQuickAnimCtrl_m_animationRoots_tag, QQuickAnimatorController, m_animationRoots, QQuickAnimCtrl_AnimRoots_t);

using QQuickAnimCtrl_RunningAnimators_t = QSet<QQuickAnimatorJob *>;
W_DECLARE_PRIVATE_MEMBER(QQuickAnimCtrl_m_runningAnimators_tag, QQuickAnimatorController, m_runningAnimators, QQuickAnimCtrl_RunningAnimators_t);

W_DECLARE_PRIVATE_MEMBER(QQuickAnimCtrl_m_window_tag, QQuickAnimatorController, m_window, QQuickWindow*);

WAYLIB_SERVER_BEGIN_NAMESPACE

// Call it before any wlroots render to clean up the GL state in Qt.
// If you don't do this, there will be tearing, flickering and other graphics problems
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

class Q_DECL_HIDDEN BufferRendererProxy : public WQuickTextureProxy
{
public:
    BufferRendererProxy(QQuickItem *parent = nullptr)
        : WQuickTextureProxy(parent) {}

    inline WBufferRenderer *renderer() const {
        auto r = qobject_cast<WBufferRenderer*>(sourceItem());
        if (sourceItem())
            Q_ASSERT(r);
        return r;
    }

    void setRenderer(WBufferRenderer *renderer) {
        auto oldRenderer = this->renderer();
        if (oldRenderer == renderer)
            return;
        if (oldRenderer)
            oldRenderer->unlockCacheBuffer(this);
        if (renderer)
            renderer->lockCacheBuffer(this);
        setSourceItem(renderer);
    }

private:
    using WQuickTextureProxy::setSourceItem;
};

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
        {

        }
        ~LayerData() {
            if (renderer) {
                QObject::disconnect(rendererConnection);
                renderer->deleteLater();
            }
            QObject::disconnect(cursorContentChangedConnection);

            if (wlrLayer) {
                delete wlrLayer;
                wlrLayer = nullptr;
            }
        }

        OutputLayer *layer;
        QPointer<qw_output_layer> wlrLayer;
        QPointer<WBufferRenderer> renderer;
        QMetaObject::Connection rendererConnection;
        QMetaObject::Connection cursorContentChangedConnection;

        // dirty state
        uint contentsIsDirty:1;
        // end

        QRectF mapRect;
        QRectF noClipMapRect;
        QRect mapToOutput;
        QSize pixelSize;
        QMatrix4x4 renderMatrix;
        quint64 cursorContentRevision = 0;

        // for proxy
        LayerData *mapFromLayer = nullptr; // check mapFrom before use
        QPointer<OutputHelper> mapFrom;
        QPointer<QQuickItem> mapTo;
    };

    OutputHelper(WOutputViewport *output, WOutputRenderWindow *parent, bool contentIsDirty)
        : WOutputHelper(output->output(), contentIsDirty, parent)
        , m_output(output)
    {

    }

    ~OutputHelper()
    {
        if (!m_layerProxys.isEmpty() || m_output || m_output2 || m_layerPorxyContainer
                || m_cursorRenderer || m_cursorLayerProxy)
            qFatal("Before destroying OutputHelper, ensure call invalidate method.");
    }

    inline void init() {
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
        cleanLayerCompositor();
        cleanCursorRender();
        qDeleteAll(m_layers);
    }

    inline qreal devicePixelRatio() const {
        return m_output->devicePixelRatio();
    }

    void updateSceneDPR();

    int indexOfLayer(OutputLayer *layer) const;
    LayerData *getLayer(OutputLayer *layer) const;
    bool attachLayer(OutputLayer *layer);
    void detachLayer(OutputLayer *layer);
    void sortLayers();
    void cleanLayerCompositor();
    void cleanCursorRender();

    inline qw_buffer *beginRender(WBufferRenderer *renderer,
                                 const QSize &pixelSize, uint32_t format,
                                 WBufferRenderer::RenderFlags flags);
    inline bool render(WBufferRenderer *renderer, int sourceIndex, const QMatrix4x4 &renderMatrix,
                       const QRectF &sourceRect, const QRectF &viewportRect, bool preserveColorContents);

    static bool visualizeLayers() {
        static bool on = qEnvironmentVariableIsSet("WAYLIB_VISUALIZE_LAYERS");
        return on;
    }

    qw_buffer *renderLayer(LayerData *layer, bool *dontEndRenderAndReturnNeedsEndRender);
    WBufferRenderer *afterRender();
    WBufferRenderer *compositeLayers(const QVector<LayerData*> layers, bool forceShadowRenderer);
    bool commit(WBufferRenderer *buffer);
    bool tryToHardwareCursor(const LayerData *layer);

private:
    WOutputViewport *m_output = nullptr;
    QList<LayerData*> m_layers;
    WBufferRenderer *m_lastCommitBuffer = nullptr;
    // only for render cursor
    QPointer<WBufferRenderer> m_cursorRenderer;
    BufferRendererProxy *m_cursorLayerProxy = nullptr;
    bool m_cursorDirty = false;
    bool m_hardwareCursorRenderComplete = false;

    // for compositeLayers
    QPointer<WOutputViewport> m_output2;
    QPointer<QQuickItem> m_layerPorxyContainer;
    QList<QPointer<BufferRendererProxy>> m_layerProxys;
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
            qCInfo(lcWlRenderer) << "Layer" << layer->parent() << "is accepted("
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
            qCInfo(lcWlRenderer) << "Layer" << layer->parent() << "is rejected on" << output;
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

    QRhi::FrameOpResult beginFrameChecked()
    {
        QQuickRenderControl::beginFrame();

        auto *d = QQuickRenderControlPrivate::get(this);
        switch (d->frameStatus) {
        case QQuickRenderControlPrivate::RecordingFrame:
            return QRhi::FrameOpSuccess;
        case QQuickRenderControlPrivate::DeviceLostInBeginFrame:
            return QRhi::FrameOpDeviceLost;
        case QQuickRenderControlPrivate::ErrorInBeginFrame:
            return d->rhi && d->rhi->isDeviceLost()
                ? QRhi::FrameOpDeviceLost
                : QRhi::FrameOpError;
        case QQuickRenderControlPrivate::NotRecordingFrame:
            break;
        }
        return QRhi::FrameOpError;
    }

    QRhi::FrameOpResult endFrameChecked()
    {
        auto *d = QQuickRenderControlPrivate::get(this);
        if (!d->rhi
            || !d->window
            || d->frameStatus != QQuickRenderControlPrivate::RecordingFrame
            || !d->rhi->isRecordingFrame()) {
            return d->rhi && d->rhi->isDeviceLost()
                ? QRhi::FrameOpDeviceLost
                : QRhi::FrameOpError;
        }

        const auto result = d->rhi->endOffscreenFrame();
        // Match QQuickRenderControl::endFrame(), while retaining the QRhi
        // submission result which the public void API discards.
        d->frameStatus = QQuickRenderControlPrivate::NotRecordingFrame;
        Q_EMIT d->window->afterFrameEnd();
        return result;
    }

    QWindow *renderWindow(QPoint *) override {
        return m_renderWindow;
    }

    QWindow *m_renderWindow = nullptr;
};

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
        if (index < 0)
            return nullptr;
        return outputs.at(index);
    }

    int indexOfOutputLayer(const WOutputLayer *layer) const;
    inline OutputLayer *getOutputLayer(WOutputLayer *layer) const {
        int index = indexOfOutputLayer(layer);
        if (index < 0)
            return nullptr;
        return layers.at(index);
    }
    inline OutputLayer *ensureOutputLayer(WOutputLayer *layer) {
        if (auto l = getOutputLayer(layer))
            return l;
        layers.append(new OutputLayer(layer));
        return layers.last();
    }
    inline bool containsOutput(WOutput *o) const {
        for (auto output : std::as_const(outputs)) {
            if (output->output()->output() == o)
                return true;
        }
        return false;
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
    doRenderOutputs(qw_output *needsFrameOutput, const QList<OutputHelper *> &outputs,
                    bool forceRender);
    bool releaseRenderBuffer(WBufferRenderer *renderer, const char *purpose);
    bool releaseRenderBuffers(QVector<std::pair<OutputHelper *, WBufferRenderer *>> &needsCommit);
    void cleanupRetiredRenderResources(bool force);
    bool prepareTextureSamplingForRenderPass(qw_buffer *currentBuffer,
                                             const QVector<WSGTextureProvider *> &activeProviders,
                                             const char *purpose,
                                             int sourceIndex,
                                             QVector<qw_texture *> *preparedTextures);
    bool prepareTextureForCurrentRenderPass(qw_texture *texture,
                                            const char *purpose);
    bool finishTextureSamplingForRenderPass(const QVector<qw_texture *> &preparedTextures,
                                            const char *purpose,
                                            int sourceIndex);
    void setCurrentPresentationOutput(WOutput *output);
    void markSurfaceTexturedForPresentation(WSurface *surface);
    void submitPresentationFeedbackForOutput(WOutput *output);
    void clearPresentationFeedbackForOutput(WOutput *output);
    void clearPresentationFeedback();
    inline void failCurrentFrame()
    {
        frameFailed = true;
    }
    inline void failCurrentFrameFatally(const QString &message)
    {
        frameFailed = true;
        if (fatalRenderError.isEmpty())
            fatalRenderError = message;
    }
    void doRender(qw_output *needsFrameOutput, const QList<OutputHelper*> &outputs,
                  bool forceRender, bool doCommit);

    inline void pushRenderer(WBufferRenderer *renderer) {
        rendererList.push(renderer);
    }

    inline void scheduleDoRender() {
        if (!isInitialized())
            return; // Not initialized

        if (inRendering)
            return;

        for (auto o : std::as_const(outputs)) {
            o->qwoutput()->schedule_frame();
        }
    }

    Q_DECLARE_PUBLIC(WOutputRenderWindow)

    bool componentCompleted = true;
    bool inRendering = false;
    bool renderEnabled = true;
    bool frameFailed = false;
    QString fatalRenderError;

    QPointer<qw_renderer> m_renderer;
    QPointer<qw_allocator> m_allocator;
    QPointer<WPresentation> m_presentation;
    QPointer<WOutput> m_currentPresentationOutput;
    QHash<WOutput *, QVector<QPointer<WSurface>>> m_pendingPresentationSurfaces;

    QList<OutputHelper*> outputs;
    QList<OutputLayer*> layers;
    bool disableLayers = false;

    QOpenGLContext *glContext = nullptr;
#ifdef ENABLE_VULKAN_RENDER
    QScopedPointer<QVulkanInstance> vkInstance;
#endif

    QStack<WBufferRenderer*> rendererList;
    QVector<qw_texture *> *m_currentPreparedTextures = nullptr;
    QSet<qw_texture *> m_currentPreparedTextureSet;
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

OutputHelper::LayerData *OutputHelper::getLayer(OutputLayer *layer) const
{
    const auto index = indexOfLayer(layer);
    if (index < 0)
        return nullptr;
    return m_layers.at(index);
}

bool OutputHelper::attachLayer(OutputLayer *layer)
{
    Q_ASSERT(indexOfLayer(layer) < 0);
    // qw_output will destory this layer on qw_output destroy
    auto qwlayer = qw_output_layer::create(*qwoutput());
    if (!qwlayer)
        return false;

    auto layerData = new LayerData(layer, qwlayer);
    m_layers.append(layerData);

    if (auto cursorItem = qobject_cast<WQuickCursor *>(layer->layer->parent())) {
        layerData->cursorContentChangedConnection = connect(cursorItem, &WQuickCursor::contentChanged, this, [this, layerData] {
            layerData->contentsIsDirty = true;
            if (contentIsDirty())
                scheduleFrame();
            else
                update();
        });
    }

    connect(layer->layer, &WOutputLayer::zChanged, this, &OutputHelper::sortLayers);
    sortLayers();

    return true;
}

void OutputHelper::detachLayer(OutputLayer *layer)
{
    int index = indexOfLayer(layer);
    Q_ASSERT(index >= 0);
    auto l = m_layers.takeAt(index);

    if (m_cursorLayerProxy && m_cursorLayerProxy->sourceItem() == l->renderer) {
        // Clear hardware cursor
        tryToHardwareCursor(nullptr);
        cleanCursorRender();
    }

    delete l;
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
    QList<QPointer<BufferRendererProxy>> tmpList;
    tmpList.swap(m_layerProxys);

    for (auto proxy : std::as_const(tmpList)) {
        if (!proxy)
            continue;

        proxy->setRenderer(nullptr);
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

void OutputHelper::cleanCursorRender()
{
    if (m_cursorRenderer) {
        m_cursorRenderer->deleteLater();
        m_cursorRenderer = nullptr;
        m_cursorLayerProxy = nullptr;
    }
}

qw_buffer *OutputHelper::beginRender(WBufferRenderer *renderer,
                                    const QSize &pixelSize, uint32_t format,
                                    WBufferRenderer::RenderFlags flags)
{
    return renderer->beginRender(pixelSize, devicePixelRatio(), format, flags);
}

bool OutputHelper::render(WBufferRenderer *renderer, int sourceIndex, const QMatrix4x4 &renderMatrix,
                          const QRectF &sourceRect, const QRectF &targetRect, bool preserveColorContents)
{
    auto *windowPrivate = renderWindowD();
    auto *presentationOutput = m_output ? m_output->output() : nullptr;
    windowPrivate->setCurrentPresentationOutput(presentationOutput);
    windowPrivate->pushRenderer(renderer);
    const bool ok = renderer->render(sourceIndex, renderMatrix, sourceRect, targetRect, preserveColorContents);
    windowPrivate->setCurrentPresentationOutput(nullptr);
    if (!ok) {
        qCWarning(lcWlBufferRenderer) << "WBufferRenderer render pass failed"
                                      << "renderer" << renderer
                                      << "sourceIndex" << sourceIndex
                                      << "currentBuffer" << renderer->currentBuffer()
                                      << "lastBuffer" << renderer->lastBuffer();
        windowPrivate->clearPresentationFeedbackForOutput(presentationOutput);
    }
    return ok;
}

static QQuickItem *createVisualRectangle(QQuickItem *target, const QColor &color) {
    auto rectangle = new QQuickRectangle(target);
    rectangle->border()->setColor(color);
    rectangle->border()->setWidth(1);
    rectangle->setColor(Qt::transparent);
    QQuickItemPrivate::get(rectangle)->anchors()->setFill(target);

    return rectangle;
}

static inline QRectF scaleRect(const QRectF &r, qreal xScale, qreal yScale) {
    if (xScale == 1 && yScale == 1)
        return r;
    return QRectF(r.x() * xScale, r.y() * yScale, r.width() * xScale, r.height() * yScale);
}

qw_buffer *OutputHelper::renderLayer(LayerData *layer, bool *dontEndRenderAndReturnNeedsEndRender)
{
    auto source = layer->layer->layer->parent();
    if (!source->parentItem() || source->window() != renderWindow())
        return nullptr;

    if (!layer->renderer) {
        layer->renderer = new WBufferRenderer(source);
        if (visualizeLayers())
            layer->renderer->setClearColor(Qt::yellow);

        QList<QQuickItem*> sourceList {source};
        if (visualizeLayers()) {
            auto rectangle = createVisualRectangle(source, Qt::green);
            QQuickItemPrivate::get(rectangle)->refFromEffectItem(true);
            sourceList << rectangle;
        }

        layer->renderer->setSourceList(sourceList, false);
        layer->renderer->setOutput(output()->output());

        // for the new WBufferRenderer and createVisualRectangle
        renderWindowD()->updateDirtyNodes();

        layer->rendererConnection = connect(layer->renderer, &WBufferRenderer::sceneGraphChanged, this, [layer] {
            layer->contentsIsDirty = true;
        });
    }

    qreal dpr = devicePixelRatio();

    {
        QRectF mapRect, noClipMapRect;
        // matrix function: map source to WOutputViewport
        QMatrix4x4 viewportMatrix;

        const auto layerFlags = layer->layer->layer->flags();
        const bool sizeSensitive = layerFlags & WOutputLayer::SizeSensitive;
        const bool cursorLayer = layerFlags & WOutputLayer::Cursor;
        const bool isRef = layer->mapFrom && layer->mapTo;
        if (cursorLayer) {
            viewportMatrix = output()->mapToViewport(source->parentItem());
        } else if (isRef) {
            viewportMatrix = output()->mapToViewport(layer->mapTo);
            const auto xScale = layer->mapTo->width() / layer->mapFrom->output()->width();
            const auto yScale = layer->mapTo->height() / layer->mapFrom->output()->height();

            // geometry relative the other output buffer
            noClipMapRect = scaleRect(layer->mapFromLayer->noClipMapRect, xScale, yScale);
            if (sizeSensitive) {
                mapRect = scaleRect(layer->mapFromLayer->mapRect, xScale, yScale);
            } else {
                mapRect = noClipMapRect;
            }
        } else {
            viewportMatrix = output()->mapToViewport(source->parentItem());

            // geometry relative source's parent
            noClipMapRect = QRectF(source->position(), source->size());
            mapRect = noClipMapRect;
        }

        // matrix function: map source to output buffer
        const auto outputMatrix = viewportMatrix * output()->sourceRectToTargetRectTransfrom();
        if (cursorLayer) {
            auto cursorItem = qobject_cast<WQuickCursor *>(source);
            if (!cursorItem || !cursorItem->cursor())
                return nullptr;

            const auto cursorRevision = cursorItem->contentRevision();
            if (layer->cursorContentRevision != cursorRevision) {
                layer->cursorContentRevision = cursorRevision;
                layer->contentsIsDirty = true;
            }

            const QPointF outputLocalPosition = cursorItem->cursor()->position()
                                                - QPointF(output()->output()->position());
            noClipMapRect = QRectF(outputLocalPosition - cursorItem->hotSpot(), source->size());
            mapRect = noClipMapRect;
        } else {
            noClipMapRect = outputMatrix.mapRect(noClipMapRect);
            mapRect = outputMatrix.mapRect(mapRect);
        }

        QTransform revertScaleTransform;
        if (!sizeSensitive) {
            const auto scaledPoint1 = viewportMatrix.map(QPointF(0, 0));
            const auto scaledPoint2 = viewportMatrix.map(QPointF(1, 1)) - scaledPoint1;
            const auto xScale = 1.0 / std::abs(scaledPoint2.x());
            const auto yScale = 1.0 / std::abs(scaledPoint2.y());

            if (xScale != 1 || yScale != 1) {
                revertScaleTransform.scale(xScale, yScale);
                noClipMapRect.setSize(revertScaleTransform.mapRect(noClipMapRect).size());
                mapRect.setSize(revertScaleTransform.mapRect(mapRect).size());
            }
        } else if (layer->mapFrom) {
            // This layer's size is strict mode, needs follow the map source's DPR.
            dpr = layer->mapFrom->devicePixelRatio();
        }

        // clip to WOutputViewport
        mapRect = mapRect & QRectF(QPointF(0, 0), output()->size());

        QSize pixelSize;
        const auto tmpSize = mapRect.size() * dpr;

        if (layerFlags & WOutputLayer::DontClip) {
            pixelSize.rwidth() = qCeil(tmpSize.width());
            pixelSize.rheight() = qCeil(tmpSize.height());
        } else {
            // Limitation max buffer
            const auto maxSize = qMax(source->width(), source->height()) * dpr;
            pixelSize.rwidth() = qCeil(qMin(tmpSize.width(), maxSize));
            pixelSize.rheight() = qCeil(qMin(tmpSize.height(), maxSize));
        }

        if (mapRect.isEmpty()) {
            return nullptr;
        }
        Q_ASSERT(!pixelSize.isEmpty());

        QMatrix4x4 renderMatrix = cursorLayer ? QMatrix4x4() : revertScaleTransform * viewportMatrix;
        if (isRef) {
            renderMatrix = layer->mapFromLayer->renderMatrix * renderMatrix;
        }

        // viewportMatrix is relative of the output buffer, but the layer
        // render buffer's pixelSize is not same as the output buffer, so
        // needs reset the x,y translate relative the render buffer of the layer.
        if (!renderMatrix.isIdentity()) {
            const auto tmp = renderMatrix.mapRect(QRectF(QPointF(0, 0), source->size()));
            renderMatrix(0, 3) -= tmp.x();
            renderMatrix(1, 3) -= tmp.y();
        }

        std::swap(mapRect, layer->mapRect);
        std::swap(noClipMapRect, layer->noClipMapRect);
        std::swap(pixelSize, layer->pixelSize);
        std::swap(renderMatrix, layer->renderMatrix);

        if (layer->pixelSize != pixelSize
            || layer->mapRect.size() != mapRect.size()
            || layer->renderMatrix != renderMatrix) {
            layer->contentsIsDirty = true;
        }
    }

    layer->mapToOutput = QRect((layer->mapRect.topLeft() * dpr).toPoint(), layer->pixelSize);
    auto buffer = layer->renderer->lastBuffer();

    if (!buffer || layer->contentsIsDirty) {
        layer->renderer->setSize(layer->pixelSize / dpr);

        const bool alpha = !layer->layer->layer->flags().testFlag(WOutputLayer::NoAlpha);
        // Don't use OutputHelper::beginRender, because the dpr maybe is from LayerData::mapFrom
        buffer = layer->renderer->beginRender(layer->pixelSize, dpr,
                                              // TODO: Allows control format by WOutputLayer
                                              alpha ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888,
                                              WBufferRenderer::DontConfigureSwapchain);
        if (buffer) {
            const QRectF sr = QRectF(layer->mapRect.topLeft() - layer->noClipMapRect.topLeft(), layer->mapRect.size());
            const QRectF tr(QPointF(0, 0), layer->mapRect.size());

            bool renderOk = render(layer->renderer, 0, layer->renderMatrix, sr, tr,
                                   layer->layer->layer->flags().testFlag(WOutputLayer::PreserveColorContents));

            if (visualizeLayers())
                renderOk = renderOk && render(layer->renderer, 1, layer->renderMatrix, sr, tr, true);

            if (!renderOk) {
                qCWarning(lcWlBufferRenderer) << "Skipping layer buffer because render pass failed"
                                              << "renderer" << layer->renderer
                                              << "currentBuffer" << layer->renderer->currentBuffer()
                                              << "layer" << layer->layer;
                (void)renderWindowD()->releaseRenderBuffer(layer->renderer, "layer-render-buffer-abort");
                layer->renderer->endRender();
                if (dontEndRenderAndReturnNeedsEndRender)
                    *dontEndRenderAndReturnNeedsEndRender = false;
                return nullptr;
            }

            if (dontEndRenderAndReturnNeedsEndRender) {
                *dontEndRenderAndReturnNeedsEndRender = true;
            } else {
                if (!renderWindowD()->releaseRenderBuffer(layer->renderer, "layer-render-buffer")) {
                    layer->renderer->endRender();
                    return nullptr;
                }
                layer->renderer->endRender();
            }
        } else if (dontEndRenderAndReturnNeedsEndRender) {
            *dontEndRenderAndReturnNeedsEndRender = false;
        }
    }

    layer->contentsIsDirty = false;

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
        auto buffer = renderLayer(i, &needsEndBuffer);
        if (!buffer)
            continue;

        if (needsEndBuffer
            && !renderWindowD()->releaseRenderBuffer(i->renderer, "layer-render-buffer")) {
            i->renderer->endRender();
            continue;
        }

        layers.append({
            .layer = i->wlrLayer->handle(),
            .buffer = buffer->handle(),
            .src_box = {},
            .dst_box = {
                .x = i->mapToOutput.x(),
                .y = i->mapToOutput.y(),
                .width = i->mapToOutput.width(),
                .height = i->mapToOutput.height(),
            },
            .damage = &i->renderer->damageRing()->handle()->current,
            .accepted = false
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
        cleanCursorRender();
        if (m_hardwareCursorRenderComplete) {
            tryToHardwareCursor(nullptr);
        }
        return bufferRenderer();
    }

    if (output()->offscreen()) {
        cleanLayerCompositor();
        return bufferRenderer();
    }

    static bool noHardwareLayers = qEnvironmentVariableIsSet("WAYLIB_NO_HARDWARE_LAYERS");
    const bool ok = !noHardwareLayers && WOutputHelper::testCommit(bufferRenderer()->currentBuffer(), layers);
    int needsSoftwareCompositeBeginIndex = -1;
    int needsSoftwareCompositeEndIndex = -1;
    bool forceShadowRender = false;
    bool hasHardwareCursor = false;

    {
        // try fallback to cursor plane for the top layer
        auto topLayer = needsCompositeLayers.last();
        if ((!output()->disableHardwareLayers() || topLayer->layer->forceLayer())
            && !(ok && layers.last().accepted)
            && (topLayer->layer->layer->flags() & WOutputLayer::Cursor)) {
            if (tryToHardwareCursor(topLayer)) {
                Q_ASSERT(topLayer->renderer->lastBuffer()->handle() == layers.last().buffer);
                Q_ASSERT(!hasHardwareCursor);
                hasHardwareCursor = true;
                bool ok = topLayer->layer->accept(output(), true);
                Q_ASSERT(ok);
                needsCompositeLayers.removeLast();
                layers.removeLast();
            }
        }
    }

    Q_ASSERT(needsCompositeLayers.size() == layers.size());
    for (int i = layers.length() - 1; i >= 0; --i) {
        const auto &state = layers.at(i);
        Q_ASSERT(state.buffer);
        OutputLayer *layer = needsCompositeLayers[i]->layer;

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
                needsSoftwareCompositeEndIndex = i;
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

    if (!hasHardwareCursor && m_cursorRenderer) {
        // Clear hardware cursor
        tryToHardwareCursor(nullptr);
        // Don't cleanCursorRender(), maybe will use in next frame
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
                                     && WRenderHelper::getGraphicsApi(renderWindowD()->rc()) != QSGRendererInterface::Vulkan;
    if (forceShadowRenderer && !usingShadowRenderer) {
        qCDebug(lcWlBufferRenderer) << "Disabled shadow layer compositor on Vulkan; compositing layers into current output buffer"
                                    << "output" << m_output
                                    << "layerCount" << layers.size();
    }

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
            m_layerProxys.append(new BufferRendererProxy(m_layerPorxyContainer));

        auto outputProxy = m_layerProxys.first();
        outputProxy->setRenderer(bufferRenderer());
        outputProxy->setSize(output->size());
        outputProxy->setPosition({0, 0});
        outputProxy->setZ(0);
    } else {
        output = m_output;

        if (m_layerProxys.size() < layers.size())
            m_layerProxys.reserve(layers.size());

        WOutputViewportPrivate::get(output)->setExtraRenderSource(m_layerPorxyContainer);
    }

    m_layerPorxyContainer->setSize(output->size());

    for (int i = 0; i < layers.count(); ++i) {
        const int j = i + (usingShadowRenderer ? 1 : 0);
        BufferRendererProxy *proxy = nullptr;
        if (j < m_layerProxys.size()) {
            proxy = m_layerProxys.at(j);
        } else {
            proxy = new BufferRendererProxy(m_layerPorxyContainer);
            if (visualizeLayers())
                createVisualRectangle(proxy, Qt::red);
            m_layerProxys.append(proxy);
        }

        LayerData *layer = layers.at(i);
        proxy->setRenderer(layer->renderer);
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

    // for the new QQuickItem
    renderWindowD()->updateDirtyNodes();

    if (usingShadowRenderer) {
        const bool ok = beginRender(bufferRenderer2(), m_output->output()->size(),
                                    qwoutput()->handle()->render_format,
                                    WBufferRenderer::RedirectOpenGLContextDefaultFrameBufferObject);

        if (ok) {
            // stop primary render
            if (bufferRenderer()->currentBuffer()) {
                if (!renderWindowD()->releaseRenderBuffer(bufferRenderer(), "shadow-source-render-buffer")) {
                    bufferRenderer()->endRender();
                    return nullptr;
                }
                bufferRenderer()->endRender();
            }
            if (!render(bufferRenderer2(), 0, {}, m_output->effectiveSourceRect(), m_output->targetRect(), true)) {
                qCWarning(lcWlBufferRenderer) << "Skipping shadow composite buffer because render pass failed"
                                              << "renderer" << bufferRenderer2()
                                              << "currentBuffer" << bufferRenderer2()->currentBuffer()
                                              << "output" << m_output;
                (void)renderWindowD()->releaseRenderBuffer(bufferRenderer2(), "shadow-composite-render-buffer-abort");
                bufferRenderer2()->endRender();
                return nullptr;
            }

            return bufferRenderer2();
        }
    } else {
        if (bufferRenderer()->currentBuffer()) {
            if (!render(bufferRenderer(), 1, {}, m_output->effectiveSourceRect(), m_output->targetRect(), true)) {
                qCWarning(lcWlBufferRenderer) << "Skipping layer composite into output because render pass failed"
                                              << "renderer" << bufferRenderer()
                                              << "currentBuffer" << bufferRenderer()->currentBuffer()
                                              << "output" << m_output;
                (void)renderWindowD()->releaseRenderBuffer(bufferRenderer(), "layer-composite-render-buffer-abort");
                bufferRenderer()->endRender();
                return nullptr;
            }
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

bool OutputHelper::tryToHardwareCursor(const LayerData *layer)
{
    do {
        auto set_cursor = qwoutput()->handle()->impl->set_cursor;
        auto buffer = layer && layer->renderer->lastBuffer()
                          ? layer->renderer->lastBuffer()->handle()
                          : nullptr;
        if (!buffer) {
            if (!m_hardwareCursorRenderComplete)
                return true;

            if (set_cursor)
                set_cursor(qwoutput()->handle(), buffer, 0, 0);
            m_hardwareCursorRenderComplete = false;
            return true;
        }

        if (qwoutput()->handle()->software_cursor_locks > 0) {
            qCDebug(lcWlCursor) << "Skipping hardware cursor because software cursor is locked"
                                << "output" << output()->output()
                                << "locks" << qwoutput()->handle()->software_cursor_locks;
            break;
        }

        auto move_cursor = qwoutput()->handle()->impl->move_cursor;

        if (!set_cursor || !move_cursor)
            break;

        QSize pixelSize = QSize(buffer->width, buffer->height);
        auto get_cursor_sizes = qwoutput()->handle()->impl->get_cursor_sizes;
        auto get_cursor_formsts = qwoutput()->handle()->impl->get_cursor_formats;
        bool needsRepaintCursor = get_cursor_sizes && get_cursor_formsts;

        if (Q_UNLIKELY(lcWlRenderer().isDebugEnabled()))
            qCDebug(lcWlRenderer) << "Request cursor buffer size:" << pixelSize;

        if (get_cursor_sizes) {
            if (Q_UNLIKELY(lcWlRenderer().isDebugEnabled()))
                qCDebug(lcWlRenderer) << "Supported cursor sizes for output" << output() << ":";

            bool foundTargetSize = false;
            size_t sizes_len = 0;
            const auto sizes = get_cursor_sizes(qwoutput()->handle(), &sizes_len);
            for (size_t i = 0; i < sizes_len; ++i) {
                if (Q_UNLIKELY(lcWlRenderer().isDebugEnabled()))
                     qCDebug(lcWlRenderer) << "    " << sizes[i].width << "x" << sizes[i].height;

                if (sizes[i].width == pixelSize.width()
                    && sizes[i].height == pixelSize.height()) {
                    foundTargetSize = true;
                    if (Q_UNLIKELY(lcWlRenderer().isDebugEnabled()))
                        qCDebug(lcWlRenderer) << "    " << sizes[i].width << "x" << sizes[i].height << "(matched)";
                    else
                        break; //need continue to print other sizes when find matched size
                }
            }

            if (!foundTargetSize && sizes_len > 0) {
                // Use the wlroots's backend supported size to re-render the cursor
                for (size_t i = 0; i < sizes_len; ++i) {
                    if (sizes[i].width > pixelSize.width()
                        && sizes[i].height > pixelSize.height()) {
                        pixelSize.rwidth() = sizes[i].width;
                        pixelSize.rheight() = sizes[i].height;
                        if (Q_UNLIKELY(lcWlRenderer().isDebugEnabled()))
                            qCDebug(lcWlRenderer) << "Re-render cursor with size" << pixelSize
                                                 << "because the original cursor size"
                                                 << QSize(buffer->width, buffer->height)
                                                 << "is not supported by wlroots backend";
                        needsRepaintCursor = true;
                        break;
                    }
                }

                if (Q_UNLIKELY(!needsRepaintCursor)) {
                    qCWarning(lcWlRenderer) << "Can't find suitable cursor size for" << pixelSize;
                    return false;
                }
            } else {
                needsRepaintCursor = false;
            }
        }

        if (needsRepaintCursor) {
            // needs render cursor again
            if (!m_cursorRenderer) {
                m_cursorRenderer = new WBufferRenderer(renderWindow()->contentItem());
                if (visualizeLayers())
                    m_cursorRenderer->setClearColor(Qt::cyan);
                m_cursorLayerProxy = new BufferRendererProxy(m_cursorRenderer);
                m_cursorRenderer->setSourceList({m_cursorLayerProxy}, false);
                m_cursorRenderer->setOutput(m_output->output());
                m_cursorRenderer->setVisible(false);
                // for the new WBufferRenderer and WQuickTextureProxy
                renderWindowD()->updateDirtyNodes();

                connect(m_cursorRenderer, &WBufferRenderer::sceneGraphChanged, this, [this] {
                    m_cursorDirty = true;
                });
            }
            m_cursorLayerProxy->setRenderer(layer->renderer);
            // Ensure render size same as source buffer size
            m_cursorLayerProxy->setWidth(buffer->width);
            m_cursorLayerProxy->setHeight(buffer->height);

            auto newBuffer = m_cursorRenderer->lastBuffer();

            // When the cursor moves to the edge of the screen, it will be truncated by default,
            // and the part beyond the screen will not be included in the pixelSize. In this case,
            // the pixelSize is inconsistent with the newBuffer size, and it needs to be redrawn.
            if (m_cursorDirty || !newBuffer ||
                (pixelSize.width() != newBuffer->handle()->width) ||
                (pixelSize.height() != newBuffer->handle()->height)) {
                newBuffer = m_cursorRenderer->beginRender(pixelSize, 1.0, DRM_FORMAT_ARGB8888,
                                                          WBufferRenderer::UseCursorFormats);
                if (newBuffer) {
                    if (!m_cursorRenderer->render(0, {})) {
                        qCWarning(lcWlBufferRenderer) << "Skipping cursor buffer because render pass failed"
                                                      << "renderer" << m_cursorRenderer
                                                      << "currentBuffer" << m_cursorRenderer->currentBuffer();
                        (void)renderWindowD()->releaseRenderBuffer(m_cursorRenderer, "cursor-render-buffer-abort");
                        newBuffer = nullptr;
                    } else if (!renderWindowD()->releaseRenderBuffer(m_cursorRenderer, "cursor-render-buffer")) {
                        newBuffer = nullptr;
                    }
                    m_cursorRenderer->endRender();
                }

                m_cursorDirty = !newBuffer;
            }

            if (newBuffer) {
                Q_ASSERT(pixelSize.width() == newBuffer->handle()->width);
                Q_ASSERT(pixelSize.height() == newBuffer->handle()->height);
                buffer = newBuffer->handle();
            } else {
                buffer = nullptr;
            }
        }

        const auto hotSpot = layer->renderMatrix.map(layer->layer->layer->cursorHotSpot()
                                                     * devicePixelRatio()).toPoint();
        if (!set_cursor(qwoutput()->handle(), buffer, hotSpot.x(), hotSpot.y())) {
            break;
        } else {
            m_hardwareCursorRenderComplete = true;
            resetGlState();
        }

        const auto pos = layer->mapToOutput.topLeft() + hotSpot;
        wlr_box cleanTransform {.x = pos.x(), .y = pos.y(), .width = 0, .height = 0};
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
                     q, [q, this] {
        if (inRendering)
            return;
        q->update();
    });

    // for WSeat::filterUnacceptedEvent
    auto eventJunkman = new WEventJunkman(contentItem);
    QQuickItemPrivate::get(eventJunkman)->anchors()->setFill(contentItem);
    eventJunkman->setZ(std::numeric_limits<qreal>::lowest());

    Q_EMIT q->initialized();
}

void WOutputRenderWindowPrivate::init(OutputHelper *helper)
{
    if (graphicsApi() == QSGRendererInterface::Vulkan
        && !helper->output()->output()->forceSoftwareCursor()) {
        qCInfo(lcWlCursor) << "Forcing software cursor for Vulkan render path"
                           << "output" << helper->output()->output();
        helper->output()->output()->setForceSoftwareCursor(true);
    }

    QMetaObject::invokeMethod(helper, &WOutputHelper::scheduleFrame, Qt::QueuedConnection);
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

[[maybe_unused]] inline static QByteArrayList fromCStyleList(size_t count, const char **list) {
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

        auto phdev = qw_vulkan::rendererPhysicalDevice(m_renderer);
        auto dev = qw_vulkan::rendererDevice(m_renderer);
        auto queue_family = qw_vulkan::rendererQueueFamily(m_renderer);

#if QT_VERSION > QT_VERSION_CHECK(6, 6, 0)
        auto instance = qw_vulkan::rendererInstance(m_renderer);
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
        qCWarning(lcWlRenderer, "WOutput::initRhi: Failed to initialize QRhi");
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

    WVulkanTrace::activate(rhiSupport->rhiBackend() == QRhi::Vulkan);

    return true;
}

void WOutputRenderWindowPrivate::updateSceneDPR()
{
    W_Q(WOutputRenderWindow);
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
    Q_EMIT q->effectiveDevicePixelRatioChanged(maxDPR);
}

void WOutputRenderWindowPrivate::sortOutputs()
{
    std::stable_sort(outputs.begin(), outputs.end(),
                     [] (const OutputHelper *o1, const OutputHelper *o2) {
        return o2->output()->depends().contains(o1->output());
    });
}

QVector<std::pair<OutputHelper*, WBufferRenderer*>>
WOutputRenderWindowPrivate::doRenderOutputs(qw_output *needsFrameOutput, const QList<OutputHelper*> &outputs,
                                            bool forceRender)
{
    QVector<OutputHelper*> renderResults;
    renderResults.reserve(outputs.size());
    for (OutputHelper *helper : std::as_const(outputs)) {
        if (Q_LIKELY(needsFrameOutput)) {
            if (helper->qwoutput() != needsFrameOutput)
                continue;
            else
                Q_ASSERT(!helper->framePending());
        }

        if (Q_LIKELY(!forceRender)) {
            if (helper->framePending())
                continue;

            // Render if output will be enabled OR has extraState to commit
            // Note: Even when disabling, we need to render once to commit the disabled state
            // (extraState will contain the ENABLED=false change)
            bool shouldRender = helper->willBeEnabled() || helper->extraState();
            if (Q_UNLIKELY(!WOutputViewportPrivate::get(helper->output())->renderable())
                || !shouldRender)
                continue;

            if (!(helper->needsFrame() || helper->contentIsDirty()))
                continue;

            if (!helper->contentIsDirty()) {
                renderResults.append(helper);
                continue;
            }
        }

        Q_ASSERT(helper->output()->output()->scale() <= helper->output()->devicePixelRatio());

        const auto &format = helper->qwoutput()->handle()->render_format;
        const auto renderMatrix = helper->output()->renderMatrix();

        // maybe using the other WOutputViewport's QSGTextureProvider
        if (!helper->output()->depends().isEmpty())
            updateDirtyNodes();

        qw_buffer *buffer = helper->beginRender(helper->bufferRenderer(), helper->output()->output()->size(), format,
                                                WBufferRenderer::RedirectOpenGLContextDefaultFrameBufferObject);
        Q_ASSERT(buffer == helper->bufferRenderer()->currentBuffer());
        if (buffer) {
            if (!helper->render(helper->bufferRenderer(), 0, renderMatrix,
                                helper->output()->effectiveSourceRect(),
                                helper->output()->targetRect(),
                                helper->output()->preserveColorContents())) {
                qCWarning(lcWlBufferRenderer) << "Skipping output buffer because render pass failed"
                                              << "renderer" << helper->bufferRenderer()
                                              << "currentBuffer" << helper->bufferRenderer()->currentBuffer()
                                              << "output" << helper->output();
                if (!releaseRenderBuffer(helper->bufferRenderer(), "compositor-render-buffer-abort")) {
                    failCurrentFrameFatally(QStringLiteral("Failed to release an aborted Vulkan output render buffer"));
                }
                helper->bufferRenderer()->endRender();
                helper->resetState();
                if (frameFailed)
                    break;
                continue;
            }
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

bool WOutputRenderWindowPrivate::releaseRenderBuffer(WBufferRenderer *renderer, const char *purpose)
{
    if (WRenderHelper::getGraphicsApi(rc()) != QSGRendererInterface::Vulkan)
        return true;

    if (!renderer || !renderer->currentBuffer())
        return true;

    if (!renderer->m_renderHelper) {
        qCWarning(lcWlRenderHelper) << "Vulkan render buffer release failed: missing render helper"
                                    << "purpose" << purpose
                                    << "renderer" << renderer
                                    << "qwBuffer" << renderer->currentBuffer()
                                    << "wlrBuffer" << renderer->currentBuffer()->handle();
        return false;
    }

    const bool ok = renderer->m_renderHelper->releaseRenderBuffer(rc(),
                                                                  renderer->currentBuffer(),
                                                                  renderer->currentRenderTarget(),
                                                                  purpose);
    const bool cacheable = !purpose || !std::strstr(purpose, "abort");
    if (ok && cacheable) {
        renderer->state.renderBufferReleasedForCache = true;
        qCDebug(lcWlRenderHelper) << "Marked Vulkan render buffer released for cache sampling"
                                  << "purpose" << purpose
                                  << "renderer" << renderer
                                  << "qwBuffer" << renderer->currentBuffer()
                                  << "wlrBuffer" << renderer->currentBuffer()->handle()
                                  << "renderTarget" << renderer->currentRenderTarget();
    } else if (ok) {
        qCDebug(lcWlRenderHelper) << "Vulkan render buffer release succeeded but cache update is skipped"
                                  << "purpose" << purpose
                                  << "renderer" << renderer
                                  << "qwBuffer" << renderer->currentBuffer()
                                  << "wlrBuffer" << renderer->currentBuffer()->handle()
                                  << "renderTarget" << renderer->currentRenderTarget();
    }
    return ok;
}

bool WOutputRenderWindowPrivate::releaseRenderBuffers(QVector<std::pair<OutputHelper *, WBufferRenderer *>> &needsCommit)
{
    if (WRenderHelper::getGraphicsApi(rc()) != QSGRendererInterface::Vulkan)
        return true;

    bool ok = true;
    for (qsizetype i = 0; i < needsCommit.size();) {
        auto helper = needsCommit[i].first;
        auto renderer = needsCommit[i].second;
        if (!renderer || !renderer->currentBuffer()) {
            ++i;
            continue;
        }

        if (releaseRenderBuffer(renderer, "compositor-render-buffer")) {
            ++i;
            continue;
        }

        qCCritical(lcWlRenderHelper) << "Vulkan output frame cannot be committed because render buffer release failed"
                                     << "renderer" << renderer
                                     << "qwBuffer" << renderer->currentBuffer()
                                     << "wlrBuffer" << renderer->currentBuffer()->handle();
        ok = false;
        renderer->endRender();
        helper->resetState();
        needsCommit.removeAt(i);
    }
    return ok;
}

void WOutputRenderWindowPrivate::cleanupRetiredRenderResources(bool force)
{
    QSet<WBufferRenderer *> seenRenderers;
    auto cleanupRenderer = [&seenRenderers, force] (WBufferRenderer *renderer) {
        if (!renderer || seenRenderers.contains(renderer))
            return;

        seenRenderers.insert(renderer);
        renderer->cleanupRetiredResources(force);
    };

    for (auto helper : std::as_const(outputs)) {
        if (!helper)
            continue;

        if (helper->m_output)
            cleanupRenderer(helper->bufferRenderer());

        if (helper->m_output2)
            cleanupRenderer(helper->bufferRenderer2());

        if (helper->m_cursorRenderer)
            cleanupRenderer(helper->m_cursorRenderer);

        for (auto layer : std::as_const(helper->m_layers)) {
            if (layer)
                cleanupRenderer(layer->renderer);
        }
    }
}

bool WOutputRenderWindowPrivate::prepareTextureSamplingForRenderPass(qw_buffer *currentBuffer,
                                                                     const QVector<WSGTextureProvider *> &activeProviders,
                                                                     const char *purpose,
                                                                     int sourceIndex,
                                                                     QVector<qw_texture *> *preparedTextures)
{
    Q_ASSERT(preparedTextures);
    preparedTextures->clear();
    m_currentPreparedTextures = nullptr;
    m_currentPreparedTextureSet.clear();

    if (WRenderHelper::getGraphicsApi(rc()) != QSGRendererInterface::Vulkan)
        return true;

    m_currentPreparedTextures = preparedTextures;
    W_Q(WOutputRenderWindow);
    QSet<qw_texture *> seenTextures;
    qsizetype skippedCurrentRenderTarget = 0;
    qsizetype invalidProviderCount = 0;
    for (auto provider : activeProviders) {
        if (!provider) {
            ++invalidProviderCount;
            continue;
        }

        auto providerBuffer = provider->qwBuffer();
        if (currentBuffer && providerBuffer == currentBuffer) {
            ++skippedCurrentRenderTarget;
            WVulkanTrace::sampleDisposition(q, provider, provider->qwTexture(),
                                            WVulkanTrace::SampleDisposition::CurrentRenderTarget);
            continue;
        }

        auto sampleTexture = provider->texture();
        if (sampleTexture && currentBuffer) {
            if (auto rhiTexture = sampleTexture->rhiTexture()) {
                auto renderTargetBuffer = WRenderHelper::lookupBuffer(rhiTexture);
                if (renderTargetBuffer == currentBuffer) {
                    ++skippedCurrentRenderTarget;
                    WVulkanTrace::sampleDisposition(q, provider, provider->qwTexture(),
                                                    WVulkanTrace::SampleDisposition::CurrentRenderTarget);
                    continue;
                }
            }
        }

        auto texture = provider->qwTexture();
        if (!texture || !provider->hasTexture()) {
            ++invalidProviderCount;
            WVulkanTrace::sampleDisposition(q, provider, texture,
                                            WVulkanTrace::SampleDisposition::InvalidProvider);
            continue;
        }

        if (seenTextures.contains(texture)) {
            WVulkanTrace::sampleDisposition(q, provider, texture,
                                            WVulkanTrace::SampleDisposition::DuplicateTexture);
            continue;
        }

        seenTextures.insert(texture);
        QElapsedTimer prepareTimer;
        if (WVulkanTrace::enabled())
            prepareTimer.start();
        const bool prepareOk = WRenderHelper::prepareTextureForSampling(rc(), m_renderer,
                                                                        texture, purpose);
        const qint64 prepareUsec = prepareTimer.isValid() ? prepareTimer.nsecsElapsed() / 1000 : 0;
        WVulkanTrace::sampleDisposition(q, provider, texture,
                                        prepareOk ? WVulkanTrace::SampleDisposition::Prepared
                                                  : WVulkanTrace::SampleDisposition::PrepareFailed,
                                        prepareUsec);
        if (!prepareOk) {
            qCWarning(lcWlQtQuickTexture) << "Skipping wlroots texture for this Vulkan render pass because sampling prepare failed"
                                          << "purpose" << purpose
                                          << "sourceIndex" << sourceIndex
                                          << "currentBuffer" << currentBuffer
                                          << "currentWlrBuffer" << (currentBuffer ? currentBuffer->handle() : nullptr)
                                          << "provider" << provider
                                          << "providerBuffer" << providerBuffer
                                          << "providerWlrBuffer" << (providerBuffer ? providerBuffer->handle() : nullptr)
                                          << "qwTexture" << texture
                                          << "wlrTexture" << texture->handle()
                                          << "qtTextureSize" << (sampleTexture ? sampleTexture->textureSize() : QSize())
                                          << "activeProviderCount" << activeProviders.size()
                                          << "preparedTextureCount" << preparedTextures->size()
                                          << "skippedCurrentRenderTarget" << skippedCurrentRenderTarget
                                          << "invalidProviderCount" << invalidProviderCount;
            failCurrentFrame();
            // The pass has not drawn yet. Return ownership of every texture
            // acquired earlier in the prepass and keep all providers bound to
            // their previously valid texture.
            (void)finishTextureSamplingForRenderPass(*preparedTextures,
                                                      purpose,
                                                      sourceIndex);
            return false;
        }

        preparedTextures->append(texture);
        m_currentPreparedTextureSet.insert(texture);
    }

    return true;
}

bool WOutputRenderWindowPrivate::prepareTextureForCurrentRenderPass(qw_texture *texture,
                                                                    const char *purpose)
{
    if (WRenderHelper::getGraphicsApi(rc()) != QSGRendererInterface::Vulkan)
        return true;

    if (!texture || !m_currentPreparedTextures)
        return true;

    if (m_currentPreparedTextureSet.contains(texture))
        return true;

    const bool prepareOk = WRenderHelper::prepareTextureForSampling(rc(),
                                                                    m_renderer,
                                                                    texture,
                                                                    purpose);
    if (!prepareOk) {
        failCurrentFrame();
        return false;
    }

    m_currentPreparedTextureSet.insert(texture);
    m_currentPreparedTextures->append(texture);
    return true;
}

bool WOutputRenderWindowPrivate::finishTextureSamplingForRenderPass(const QVector<qw_texture *> &preparedTextures,
                                                                    const char *purpose,
                                                                    int sourceIndex)
{
    if (WRenderHelper::getGraphicsApi(rc()) != QSGRendererInterface::Vulkan) {
        m_currentPreparedTextures = nullptr;
        m_currentPreparedTextureSet.clear();
        return true;
    }

    W_Q(WOutputRenderWindow);
    bool ok = true;
    for (qsizetype i = preparedTextures.size() - 1; i >= 0; --i) {
        auto texture = preparedTextures.at(i);
        if (!texture)
            continue;

        QElapsedTimer finishTimer;
        if (WVulkanTrace::enabled())
            finishTimer.start();
        const bool finishOk = WRenderHelper::finishTextureSampling(rc(), m_renderer,
                                                                   texture, purpose);
        WVulkanTrace::sampleFinished(q, texture, finishOk,
                                     finishTimer.isValid() ? finishTimer.nsecsElapsed() / 1000 : 0);
        if (!finishOk) {
            qCWarning(lcWlQtQuickTexture) << "Vulkan texture sampling finish failed for render pass"
                                          << "purpose" << purpose
                                          << "sourceIndex" << sourceIndex
                                          << "qwTexture" << texture
                                          << "wlrTexture" << texture->handle()
                                          << "preparedTextureCount" << preparedTextures.size()
                                          << "releaseIndex" << i;
            ok = false;
        }
    }

    m_currentPreparedTextures = nullptr;
    m_currentPreparedTextureSet.clear();
    if (!ok) {
        failCurrentFrameFatally(QStringLiteral("Failed to return one or more sampled Vulkan textures to wlroots"));
    }
    return ok;
}

void WOutputRenderWindowPrivate::setCurrentPresentationOutput(WOutput *output)
{
    m_currentPresentationOutput = output;
}

void WOutputRenderWindowPrivate::markSurfaceTexturedForPresentation(WSurface *surface)
{
    auto *output = m_currentPresentationOutput.data();
    if (!m_presentation || !m_presentation->isValid() || !surface || !output)
        return;

    if (surface->framePacingOutput() != output)
        return;

    auto &surfaces = m_pendingPresentationSurfaces[output];
    for (const auto &existing : std::as_const(surfaces)) {
        if (existing == surface)
            return;
    }

    surfaces.append(surface);
}

void WOutputRenderWindowPrivate::submitPresentationFeedbackForOutput(WOutput *output)
{
    if (!output) {
        clearPresentationFeedback();
        return;
    }

    const auto surfaces = m_pendingPresentationSurfaces.take(output);
    if (surfaces.isEmpty() || !m_presentation || !m_presentation->isValid())
        return;

    int submittedCount = 0;
    for (const auto &surface : surfaces) {
        if (!surface || surface->framePacingOutput() != output)
            continue;

        m_presentation->surfaceTexturedOnOutput(surface, output);
        ++submittedCount;
    }

    qCDebug(lcWlPresentation) << "Submitted presentation feedback for output"
                              << "output" << output
                              << "surfaceCount" << submittedCount;
}

void WOutputRenderWindowPrivate::clearPresentationFeedbackForOutput(WOutput *output)
{
    if (output)
        m_pendingPresentationSurfaces.remove(output);
}

void WOutputRenderWindowPrivate::clearPresentationFeedback()
{
    m_pendingPresentationSurfaces.clear();
}

// ###: QQuickAnimatorController::advance symbol not export
static void QQuickAnimatorController_advance(QQuickAnimatorController *ac)
{
    bool running = false;
    for (const QSharedPointer<QAbstractAnimationJob> &job : std::as_const(W_PRIVATE_MEMBER(*ac, QQuickAnimCtrl_m_animationRoots_tag{}))) {
        if (job->isRunning()) {
            running = true;
            break;
        }
    }

    for (QQuickAnimatorJob *job : std::as_const(W_PRIVATE_MEMBER(*ac, QQuickAnimCtrl_m_runningAnimators_tag{})))
        job->commit();

    if (running)
        W_PRIVATE_MEMBER(*ac, QQuickAnimCtrl_m_window_tag{})->update();
}

void WOutputRenderWindowPrivate::doRender(qw_output *needsFrameOutput,
                                          const QList<OutputHelper *> &outputs,
                                          bool forceRender, bool doCommit)
{
    Q_ASSERT(rendererList.isEmpty());
    Q_ASSERT(!inRendering);
    if (!renderEnabled)
        return;

    inRendering = true;
    frameFailed = false;
    fatalRenderError.clear();
    W_Q(WOutputRenderWindow);
    WVulkanTrace::beginFrame(q);
    clearPresentationFeedback();

    const auto finishFrame = [this, q] (const QList<QPointer<WOutput>> &committedOutputs) {
        clearPresentationFeedback();
        m_currentPreparedTextures = nullptr;
        m_currentPreparedTextureSet.clear();

        resetGlState();

        // On Intel&Nvidia multi-GPU environment, wlroots using Intel card do render for all
        // outputs, and blit nvidia's output buffer in drm_connector_state_update_primary_fb,
        // the 'blit' behavior will make EGL context to Nvidia renderer. So must done current
        // OpenGL context here in order to ensure QtQuick always make EGL context to Intel
        // renderer before next frame.
        if (glContext)
            glContext->doneCurrent();

        WVulkanTrace::setFrameStage(q, WVulkanTrace::FrameStage::RenderEnd);
        inRendering = false;
        Q_EMIT q->renderEnd(committedOutputs);
        WVulkanTrace::endFrame(q, committedOutputs.size());

        if (!fatalRenderError.isEmpty()) {
            qCCritical(lcWlRenderer) << "Fatal Vulkan rendering failure; requesting compositor shutdown"
                                     << "reason" << fatalRenderError;
            Q_EMIT q->sceneGraphError(QQuickWindow::ContextNotAvailable,
                                      fatalRenderError);
        }
    };

    WVulkanTrace::setFrameStage(q, WVulkanTrace::FrameStage::Polish);
    for (OutputLayer *layer : std::as_const(layers)) {
        layer->beforeRender(q);
    }

    rc()->polishItems();

    const auto graphicsApi = WRenderHelper::getGraphicsApi();
    const bool isVulkan = graphicsApi == QSGRendererInterface::Vulkan;
    const bool rhiBased = QSGRendererInterface::isApiRhiBased(graphicsApi);
    if (rhiBased) {
        WVulkanTrace::setFrameStage(q, WVulkanTrace::FrameStage::BeginFrame);
        if (isVulkan) {
            const auto beginResult = rc()->beginFrameChecked();
            if (beginResult != QRhi::FrameOpSuccess) {
                if (beginResult == QRhi::FrameOpDeviceLost) {
                    failCurrentFrameFatally(QStringLiteral("Vulkan device was lost while beginning the Qt Quick frame"));
                } else {
                    failCurrentFrame();
                    qCWarning(lcWlRenderer) << "Skipping Vulkan output frame because QRhi beginOffscreenFrame failed"
                                            << "frameResult" << beginResult;
                }
                finishFrame({});
                return;
            }
        } else {
            rc()->beginFrame();
        }
    }
    WVulkanTrace::setFrameStage(q, WVulkanTrace::FrameStage::Sync);
    rc()->sync();

    WVulkanTrace::setFrameStage(q, WVulkanTrace::FrameStage::Recording);
    QQuickAnimatorController_advance(animationController.get());
    Q_EMIT q->beforeRendering();
    runAndClearJobs(&beforeRenderingJobs);

    QVector<std::pair<OutputHelper *, WBufferRenderer *>> needsCommit;
    needsCommit = doRenderOutputs(needsFrameOutput, outputs, forceRender);

    WVulkanTrace::setFrameStage(q, WVulkanTrace::FrameStage::AfterRenderingJobs);
    Q_EMIT q->afterRendering();
    runAndClearJobs(&afterRenderingJobs);

    if (rhiBased) {
        WVulkanTrace::setFrameStage(q, WVulkanTrace::FrameStage::ReleaseRenderBuffers);
        if (!releaseRenderBuffers(needsCommit)) {
            failCurrentFrameFatally(QStringLiteral("Failed to release one or more Vulkan output render buffers"));
        }
    }

    bool vulkanFrameCompleted = false;
    if (rhiBased) {
        WVulkanTrace::setFrameStage(q, WVulkanTrace::FrameStage::BeforeEndFrame);
        if (isVulkan) {
            const auto endResult = rc()->endFrameChecked();
            vulkanFrameCompleted = endResult == QRhi::FrameOpSuccess;
            if (!vulkanFrameCompleted) {
                failCurrentFrameFatally(endResult == QRhi::FrameOpDeviceLost
                                            ? QStringLiteral("Vulkan device was lost while submitting the Qt Quick frame")
                                            : QStringLiteral("Failed to submit the Vulkan Qt Quick frame"));
            }
        } else {
            rc()->endFrame();
        }
        WVulkanTrace::setFrameStage(q, WVulkanTrace::FrameStage::AfterEndFrame);
    }

    // prevent gles2-render exception in wlroots.
    // wlroots may have render operations after commit, so do
    // not move the location during the reset operation.
    // eg: screencopy ext-image-capture
    resetGlState();

    QList<QPointer<WOutput>> committedOutputs;
    WVulkanTrace::setFrameStage(q, WVulkanTrace::FrameStage::OutputCommit);
    if (doCommit || (isVulkan && frameFailed)) {
        const bool allowCommit = doCommit && !frameFailed;
        committedOutputs.reserve(needsCommit.size());
        for (auto i : std::as_const(needsCommit)) {
            auto output = i.first->output()->output();
            if (allowCommit && Q_UNLIKELY(!i.first->framePending())) {
                submitPresentationFeedbackForOutput(output);
                const quint32 sequenceBefore = output->nativeHandle()->commit_seq;
                const bool commitOk = i.first->commit(i.second);
                WVulkanTrace::outputCommitted(q, output->handle(), sequenceBefore, commitOk);
                if (Q_LIKELY(commitOk)) {
                    // Make sure the output is still valid after commit
                    if (Q_LIKELY(needsFrameOutput)) {
                        Q_ASSERT(output->handle() == needsFrameOutput);
                        if (committedOutputs.isEmpty())
                            committedOutputs.append(output);
                    } else if (!committedOutputs.contains(output)) {
                        committedOutputs.append(output);
                    }
                }
            } else {
                clearPresentationFeedbackForOutput(output);
            }

            if (i.second->currentBuffer()) {
                i.second->endRender();
            }

            i.first->resetState();
        }
    } else {
        clearPresentationFeedback();
    }

    if (vulkanFrameCompleted) {
        // endOffscreenFrame() has completed all GPU work for this frame. Run
        // retirement only after output commit/reset has transferred or dropped
        // every buffer reference, so imported QRhi wrappers are destroyed
        // before their wlroots textures and buffer locks are released.
        cleanupRetiredRenderResources(false);
    }
    finishFrame(committedOutputs);
    if (vulkanFrameCompleted)
        runAndClearJobs(&afterSwapJobs);
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

    qGuiApp->installEventFilter(this);
}

WOutputRenderWindow::~WOutputRenderWindow()
{
    WVulkanTrace::windowDestroyed(this);
    qGuiApp->removeEventFilter(this);

    renderControl()->disconnect(this);
    renderControl()->invalidate();
    delete renderControl();
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
    const bool containsOutput = d->containsOutput(output->output());
    auto newOutput = new OutputHelper(output, this, true);
    d->outputs << newOutput;
    d->sortOutputs();

    if (d->m_renderer) {
        auto qwoutput = newOutput->qwoutput();
        if (qwoutput->handle()->renderer != d->m_renderer->handle())
            qwoutput->init_render(d->m_allocator->handle(), d->m_renderer->handle());
        Q_EMIT outputViewportInitialized(output);
    }

    if (!containsOutput) {
        output->output()->safeConnect(&qw_output::notify_frame,
                                      this,
                                      qOverload<>(&WOutputRenderWindow::render));
        connect(newOutput->qwoutput(), &qw_output::notify_needs_frame,
                output->output(),
                &WOutput::scheduleFrame);
    }

    if (!d->isInitialized())
        return;

    d->updateSceneDPR();
    d->init(newOutput);
    newOutput->update();

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

    if (output->output() && !d->containsOutput(output->output())) {
        bool ok = output->output()->safeDisconnect(this);
        Q_ASSERT(ok);
        ok = disconnect(outputHelper->qwoutput(), &qw_output::notify_needs_frame,
                        output->output(),
                        &WOutput::scheduleFrame);
        Q_ASSERT(ok);
    }

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
        outputHelper->scheduleFrame();

    connect(layer, &WOutputLayer::flagsChanged,
            outputHelper, &WOutputHelper::scheduleFrame);
    connect(layer, &WOutputLayer::zChanged,
            outputHelper, &WOutputHelper::scheduleFrame);

    if (auto od = WOutputViewportPrivate::get(output)) {
        od->notifyLayersChanged();
        od->notifyHardwareLayersChanged();
    }
}

void WOutputRenderWindow::attach(WOutputLayer *layer, WOutputViewport *output,
                                 WOutputViewport *mapFrom, QQuickItem *mapTo)
{
    Q_ASSERT(mapTo->window() == this);

    attach(layer, output);
    Q_D(WOutputRenderWindow);

    OutputLayer *wapper = d->getOutputLayer(layer);
    Q_ASSERT(wapper);

    OutputHelper *helper = d->getOutputHelper(output);
    Q_ASSERT(helper);

    auto layerData = helper->getLayer(wapper);
    Q_ASSERT(layerData);

    layerData->mapFrom = d->getOutputHelper(mapFrom);
    Q_ASSERT(layerData->mapFrom);
    layerData->mapFromLayer = layerData->mapFrom->getLayer(wapper);
    Q_ASSERT(layerData->mapFromLayer);
    layerData->mapTo = mapTo;
}

void WOutputRenderWindow::detach(WOutputLayer *layer, WOutputViewport *output)
{
    Q_D(WOutputRenderWindow);

    Q_ASSERT(d->indexOfOutputLayer(layer) >= 0);
    auto wapper = d->ensureOutputLayer(layer);
    Q_ASSERT(wapper->outputs().contains(output));
    wapper->outputs().removeOne(output);

    auto outputHelper = d->getOutputHelper(output);
    if (!outputHelper)
        return;

    bool ok = layer->disconnect(outputHelper);
    Q_ASSERT(ok);

    outputHelper->detachLayer(wapper);
    outputHelper->scheduleFrame();

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

WOutputHelper *WOutputRenderWindow::getOutputHelper(WOutputViewport *output) const
{
    Q_D(const WOutputRenderWindow);
    return d->getOutputHelper(output);
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

void WOutputRenderWindow::setPresentation(WPresentation *presentation)
{
    Q_D(WOutputRenderWindow);
    d->m_presentation = presentation;
    if (!presentation)
        d->clearPresentationFeedback();
}

void WOutputRenderWindow::markSurfaceTexturedForPresentation(WSurface *surface)
{
    Q_D(WOutputRenderWindow);
    d->markSurfaceTexturedForPresentation(surface);
}

bool WOutputRenderWindow::prepareTextureSamplingForRenderPass(qw_buffer *currentBuffer,
                                                              const QVector<WSGTextureProvider *> &activeProviders,
                                                              const char *purpose,
                                                              int sourceIndex,
                                                              QVector<qw_texture *> *preparedTextures)
{
    Q_D(WOutputRenderWindow);
    return d->prepareTextureSamplingForRenderPass(currentBuffer,
                                                  activeProviders,
                                                  purpose,
                                                  sourceIndex,
                                                  preparedTextures);
}

bool WOutputRenderWindow::prepareTextureForCurrentRenderPass(qw_texture *texture,
                                                             const char *purpose)
{
    Q_D(WOutputRenderWindow);
    return d->prepareTextureForCurrentRenderPass(texture, purpose);
}

bool WOutputRenderWindow::finishTextureSamplingForRenderPass(const QVector<qw_texture *> &preparedTextures,
                                                             const char *purpose,
                                                             int sourceIndex)
{
    Q_D(WOutputRenderWindow);
    return d->finishTextureSamplingForRenderPass(preparedTextures, purpose, sourceIndex);
}

WBufferRenderer *WOutputRenderWindow::currentRenderer() const
{
    Q_D(const WOutputRenderWindow);
    return d->rendererList.isEmpty() ? nullptr : d->rendererList.top();
}

bool WOutputRenderWindow::inRendering() const
{
    Q_D(const WOutputRenderWindow);
    return d->inRendering;
}

void WOutputRenderWindow::setRenderEnabled(bool enabled) {
    Q_D(WOutputRenderWindow);
    d->renderEnabled = enabled;
}

QList<QPointer<QQuickItem>> WOutputRenderWindow::paintOrderItemList(QQuickItem *root, std::function<bool(QQuickItem*)> filter)
{
    QStack<QQuickItem *> nodes;
    QList<QPointer<QQuickItem>> result;
    nodes.push(root);
    while (!nodes.isEmpty()){
        auto node = nodes.pop();
        if (!node)
            continue;
        if (!filter || filter(node)) {
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
    d->doRender(qobject_cast<qw_output*>(sender()), d->outputs, false, true);
}

void WOutputRenderWindow::render(WOutputViewport *output, bool doCommit)
{
    Q_D(WOutputRenderWindow);
    int index = d->indexOfOutputHelper(output);
    Q_ASSERT(index >= 0);

    d->doRender(nullptr, {d->outputs.at(index)}, true, doCommit);
}

void WOutputRenderWindow::update()
{
    Q_D(WOutputRenderWindow);
    for (auto o : std::as_const(d->outputs))
        o->update();
}

void WOutputRenderWindow::update(WOutputViewport *output)
{
    Q_D(WOutputRenderWindow);
    int index = d->indexOfOutputHelper(output);
    Q_ASSERT(index >= 0);
    d->outputs.at(index)->update();
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

void WOutputRenderWindow::markItemClipRectDirty(QQuickItem *item)
{
    class MarkItemClipRectDirtyJob : public QRunnable
    {
    public:
        MarkItemClipRectDirtyJob(QQuickItem *item)
            : item(item) { }
        void run() override {
            if (!item)
                return;
            auto d = QQuickItemPrivate::get(item);
            if (auto clip = d->clipNode()) {
                clip->setClipRect(item->clipRect());
                clip->update();
            }
        }
        QPointer<QQuickItem> item;
    };

    // Delay clean the qt rhi textures.
    scheduleRenderJob(new MarkItemClipRectDirtyJob(item),
                      QQuickWindow::AfterSynchronizingStage);
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

bool WOutputRenderWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->isInputEvent() && watched->isQuickItemType()) {
        auto ie = static_cast<QInputEvent*>(event);
        auto seat = WSeat::fromInputEvent(ie);
        if (!seat) return true;
        if (seat->filterEventBeforeDisposeStage(qobject_cast<QQuickItem*>(watched), ie))
            return true;
    }

    return QQuickWindow::eventFilter(watched, event);
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputrenderwindow.cpp"
