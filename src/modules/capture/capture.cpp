// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "capture.h"

#include "modules/capture/impl/capturev1impl.h"
#include "modules/item-selector/itemselector.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "workspace/workspace.h"

#include <private/qquickitem_p.h>

#include <wlayersurface.h>
#include <woutputitem.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wquickcursor.h>
#include <wquicktextureproxy.h>
#include <wtools.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwlayershellv1.h>

#include <QLoggingCategory>
#include <QQueue>
#include <QQuickItemGrabResult>
#include <QSGTextureProvider>

#include <utility>

Q_LOGGING_CATEGORY(qLcCapture, "treeland.capture")

static inline QRectF scaledRect(const QRectF &rect, qreal devicePixelRatio)
{
    return { rect.x() * devicePixelRatio,
             rect.y() * devicePixelRatio,
             rect.width() * devicePixelRatio,
             rect.height() * devicePixelRatio };
}

CaptureSource *CaptureContextV1::source() const
{
    return m_captureSource;
}

static inline uint32_t captureSourceTypeToProtocol(CaptureSource::CaptureSourceType type)
{
    switch (type) {
    case CaptureSource::Output:
        return 0x1;
    case CaptureSource::Window:
    case CaptureSource::Surface:
        return 0x2;
    case CaptureSource::Region:
        return 0x4;
    default:
        Q_UNREACHABLE_RETURN(0);
    }
}

void CaptureContextV1::setSource(CaptureSource *source, const QRect &captureRegion)
{
    if (m_captureSource == source && m_captureRegion == captureRegion)
        return;
    if (m_captureSource) {
        m_captureSource->disconnect(this);
        if (session()) {
            m_captureSource->disconnect(session());
        }
    }
    m_captureSource = source;
    m_captureRegion = captureRegion;
    connect(m_captureSource,
            &CaptureSource::targetDestroyed,
            this,
            &CaptureContextV1::handleSourceDestroyed);
    m_handle->sendSourceReady(this->captureRegion(),
                              captureSourceTypeToProtocol(source->sourceType()));
    Q_EMIT sourceChanged();
}

void CaptureContextV1::cancelSelect()
{
    sendSourceFailed(UserCancel);
}

WSurface *CaptureContextV1::mask() const
{
    return m_handle->mask;
}

bool CaptureContextV1::freeze() const
{
    return m_handle->freeze;
}

bool CaptureContextV1::withCursor() const
{
    return m_handle->withCursor;
}

CaptureSource::CaptureSourceHint CaptureContextV1::sourceHint() const
{
    return { m_handle->sourceHint };
}

CaptureContextV1::CaptureContextV1(treeland_capture_context_v1 *h,
                                   WOutputRenderWindow *outputRenderWindow,
                                   QObject *parent)
    : QObject(parent)
    , m_handle(h)
    , m_outputRenderWindow(outputRenderWindow)
{
    connect(h, &treeland_capture_context_v1::selectSource, this, &CaptureContextV1::onSelectSource);
    connect(h, &treeland_capture_context_v1::capture, this, &CaptureContextV1::onCapture);
    connect(h, &treeland_capture_context_v1::newSession, this, &CaptureContextV1::onCreateSession);
}

void CaptureContextV1::onSelectSource()
{
    auto context = qobject_cast<treeland_capture_context_v1 *>(sender());
    Q_ASSERT(context); // Sender must be context.
    Q_EMIT selectInfoReady();
}

void CaptureContextV1::onCapture(treeland_capture_frame_v1 *frame)
{
    if (m_frame) {
        wl_client_post_implementation_error(wl_resource_get_client(m_handle->resource),
                                            "Cannot capture frame twice!");
        return;
    }
    if (!m_captureSource) {
        wl_client_post_implementation_error(wl_resource_get_client(m_handle->resource),
                                            "Source is not ready.");
        return;
    }
    m_frame = frame;
    auto notifyBuffer = [this] {
        m_frame->sendBuffer(
            WTools::drmToShmFormat(WTools::toDrmFormat(m_captureSource->image().format())),
            source()->cropRect().width(),
            source()->cropRect().height(),
            source()->cropRect().width() * 4);
        m_frame->sendBufferDone();
        connect(m_frame,
                &treeland_capture_frame_v1::copy,
                this,
                &CaptureContextV1::handleFrameCopy,
                Qt::UniqueConnection);
        connect(m_frame, &treeland_capture_frame_v1::beforeDestroy, this, [this] {
            m_frame = nullptr;
        });
    };
    if (m_captureSource->imageValid()) {
        notifyBuffer();
    } else {
        connect(m_captureSource, &CaptureSource::imageReady, this, notifyBuffer);
    }
    Q_EMIT finishSelect();
}

void CaptureContextV1::handleFrameCopy(QW_NAMESPACE::qw_buffer *buffer)
{
    if (m_captureSource) {
        m_captureSource->copyBuffer(buffer);
        m_frame->sendReady();
    } else {
        wl_client_post_implementation_error(wl_resource_get_client(m_handle->resource),
                                            "Source is not ready, cannot capture.");
    }
}

void CaptureContextV1::sendSourceFailed(SourceFailure failure)
{
    m_handle->sendSourceFailed(failure);
}

void CaptureContextV1::onCreateSession(treeland_capture_session_v1 *session)
{
    if (m_session) {
        wl_client_post_implementation_error(wl_resource_get_client(m_handle->resource),
                                            "Cannot create session twice!");
        return;
    }
    if (!m_captureSource) {
        wl_client_post_implementation_error(wl_resource_get_client(m_handle->resource),
                                            "Source is not ready.");
        return;
    }
    m_session = session;
    connect(m_session,
            &treeland_capture_session_v1::start,
            this,
            &CaptureContextV1::handleSessionStart);
    connect(m_session,
            &treeland_capture_session_v1::frameDone,
            this,
            &CaptureContextV1::handleFrameDone);
    connect(m_session, &treeland_capture_session_v1::beforeDestroy, this, [this] {
        disconnect(outputRenderWindow(),
                   &WOutputRenderWindow::renderEnd,
                   this,
                   &CaptureContextV1::handleRenderEnd);
    });
    ensureSourceSessionConnection();
    Q_EMIT finishSelect();
}

void CaptureContextV1::ensureSourceSessionConnection()
{
    Q_ASSERT(session() && source());
    connect(source(),
            &CaptureSource::bufferDestroyed,
            session(),
            &treeland_capture_session_v1::sendProduceMoreCancel,
            Qt::UniqueConnection);
    connect(source(),
            &CaptureSource::targetDestroyed,
            session(),
            &treeland_capture_session_v1::sendSourceDestroyCancel,
            Qt::UniqueConnection);
    connect(source(),
            &CaptureSource::targetResized,
            session(),
            &treeland_capture_session_v1::sendSourceResizeCancel,
            Qt::UniqueConnection);
}

void CaptureContextV1::handleSourceDestroyed()
{
    sendSourceFailed(SourceDestroyed);
}

void CaptureContextV1::handleSessionStart()
{
    m_currentFrameData.acked = true;
    moveToThread(QQuickWindowPrivate::get(outputRenderWindow())->context->thread());
    captureSource()->moveToThread(
        QQuickWindowPrivate::get(outputRenderWindow())->context->thread());
    auto conn = connect(outputRenderWindow(),
                        &WOutputRenderWindow::renderEnd,
                        this,
                        &CaptureContextV1::handleRenderEnd,
                        Qt::AutoConnection);
    if (!conn) {
        qCWarning(qLcCapture()) << "Cannot connect to render end of output render window.";
    }
    if (!outputRenderWindow()->inRendering()) {
        QMetaObject::invokeMethod(this, &CaptureContextV1::handleRenderEnd, Qt::AutoConnection);
    }
}

void CaptureContextV1::handleFrameDone(uint32_t tvSecHi, uint32_t tvSecLo, uint32_t tvUsec)
{
    if (m_currentFrameData.readyAt.tvSecHi == tvSecHi
        && m_currentFrameData.readyAt.tvSecLo == tvSecLo
        && m_currentFrameData.readyAt.tvUsec == tvUsec) {
        // Note: dmabuf attributes is exported from output backing buffer, fds will be
        // closed as soon as backing buffer is destroyed. We should not close fd here.
        m_currentFrameData.acked = true;
    } else {
        qCWarning(qLcCapture())
            << "Receive a frame done event that is not corresponding to current frame timestamp.";
    }
}

QPointer<treeland_capture_session_v1> CaptureContextV1::session() const
{
    return m_session;
}

QPointer<CaptureSource> CaptureContextV1::captureSource() const
{
    return m_captureSource;
}

QPointer<WOutputRenderWindow> CaptureContextV1::outputRenderWindow() const
{
    return m_outputRenderWindow;
}

void CaptureContextV1::handleRenderEnd()
{
    if (!session() || !m_currentFrameData.acked)
        return;
    auto source = captureSource();
    Q_ASSERT(source);
    auto dmabuf = source->sourceDMABuffer();
    if (!dmabuf) {
        qCWarning(qLcCapture()) << "Source has been invalid while connection still exists.";
        return;
    }
    m_currentFrameData = {};
    dmabuf->get_dmabuf(&m_currentFrameData.attribs);

    union
    {
        uint64_t modifier;

        struct
        {
            uint32_t mod_low;
            uint32_t mod_high;
        };
    } modifierUnion(m_currentFrameData.attribs.modifier);

    qInfo() << "session:" << session();
    qInfo() << "session resource:" << session()->resource;
    treeland_capture_session_v1_send_frame(session()->resource,
                                           source->cropRect().x(),
                                           source->cropRect().y(),
                                           m_currentFrameData.attribs.width,
                                           m_currentFrameData.attribs.height,
                                           0,
                                           0,
                                           m_currentFrameData.attribs.format,
                                           modifierUnion.mod_high,
                                           modifierUnion.mod_low,
                                           m_currentFrameData.attribs.n_planes);
    for (auto i = 0; i < m_currentFrameData.attribs.n_planes; ++i) {
        treeland_capture_session_v1_send_object(session()->resource,
                                                i,
                                                m_currentFrameData.attribs.fd[i],
                                                m_currentFrameData.attribs.stride[i]
                                                    * m_currentFrameData.attribs.height,
                                                m_currentFrameData.attribs.offset[i],
                                                m_currentFrameData.attribs.stride[i],
                                                i);
    }
    gettimeofday(&m_currentFrameData.readyAt.tv, nullptr);
    treeland_capture_session_v1_send_ready(session()->resource,
                                           m_currentFrameData.readyAt.tvSecHi,
                                           m_currentFrameData.readyAt.tvSecLo,
                                           m_currentFrameData.readyAt.tvUsec);
}

CaptureManagerV1::CaptureManagerV1(QObject *parent)
    : QObject(parent)
    , m_manager(nullptr)
    , m_captureContextModel(new CaptureContextModel(this))
    , m_contextInSelection(nullptr)
{
}

void CaptureManagerV1::setSelector(CaptureSourceSelector *selector)
{
    if (selector == m_selector)
        return;
    m_selector = selector;
    Q_EMIT selectorChanged();
}

WOutputRenderWindow *CaptureManagerV1::outputRenderWindow() const
{
    return m_outputRenderWindow;
}

void CaptureManagerV1::setOutputRenderWindow(WOutputRenderWindow *renderWindow)
{
    if (m_outputRenderWindow == renderWindow) {
        return;
    }
    m_outputRenderWindow = renderWindow;
}

QByteArrayView CaptureManagerV1::interfaceName() const
{
    return treeland_capture_manager_v1_interface.name;
}

QPointer<WToplevelSurface> CaptureManagerV1::maskShellSurface() const
{
    return m_maskShellSurface;
}

QPointer<SurfaceWrapper> CaptureManagerV1::maskSurfaceWrapper() const
{
    return m_maskSurfaceWrapper;
}

void CaptureManagerV1::create(WServer *server)
{
    m_manager = new treeland_capture_manager_v1(server->handle()->handle(), this);
    connect(m_manager,
            &treeland_capture_manager_v1::newCaptureContext,
            this,
            [this](treeland_capture_context_v1 *context) {
                auto quickContext = new CaptureContextV1(context, outputRenderWindow(), this);
                m_captureContextModel->addContext(quickContext);
                connect(context,
                        &treeland_capture_context_v1::beforeDestroy,
                        quickContext,
                        [this, quickContext] {
                            m_captureContextModel->removeContext(quickContext);
                            handleContextBeforeDestroy(quickContext);
                            quickContext->deleteLater();
                        });
                connect(quickContext,
                        &CaptureContextV1::selectInfoReady,
                        this,
                        &CaptureManagerV1::onCaptureContextSelectSource);
            });
}

void CaptureManagerV1::destroy([[maybe_unused]] WServer *server)
{
    this->disconnect();
}

void CaptureManagerV1::onCaptureContextSelectSource()
{
    CaptureContextV1 *context = qobject_cast<CaptureContextV1 *>(sender());
    Q_ASSERT(context); // Sender must be context.
    if (contextInSelection()) {
        context->sendSourceFailed(CaptureContextV1::SelectorBusy);
        return;
    }
    m_contextInSelection = context;
    if (context->freeze()) {
        freezeAllCapturedSurface(true, context->mask());
    }
    Q_EMIT contextInSelectionChanged();
}

void CaptureManagerV1::clearContextInSelection(CaptureContextV1 *context)
{
    if (m_contextInSelection == context) {
        if (m_contextInSelection->freeze()) {
            freezeAllCapturedSurface(false, context->mask());
        }
        m_contextInSelection = nullptr;
        Q_EMIT contextInSelectionChanged();
    }
}

WSurfaceItem *closestSurfaceItem(QQuickItem *content)
{
    while (content) {
        auto item = qobject_cast<WSurfaceItem *>(content);
        if (item)
            return item;
        content = content->parentItem();
    }
    return nullptr;
}

void CaptureManagerV1::freezeAllCapturedSurface(bool freeze, WSurface *mask)
{
    // Exclude cursor surface item and the mask
    Q_ASSERT(m_outputRenderWindow);
    QQueue<QQuickItem *> nodes;
    nodes.enqueue(m_outputRenderWindow->contentItem());
    while (!nodes.isEmpty()) {
        auto node = nodes.dequeue();
        if (auto content = qobject_cast<WSurfaceItemContent *>(node)) {
            if (auto cursor = qobject_cast<WQuickCursor *>(node->parentItem())) {
                if (freeze)
                    m_frozenCursorPos = cursor->position(); // Just store position for cursor
            } else if (!mask
                       || (content->surface() != mask
                           && !mask->subsurfaces().contains(content->surface()))) {
                content->setLive(!freeze);
            } else if (content->surface() == mask) {
                auto surfaceItem = closestSurfaceItem(content);
                m_maskSurfaceWrapper = qobject_cast<SurfaceWrapper *>(surfaceItem->parentItem());
                if (m_maskSurfaceWrapper) {
                    m_maskSurfaceWrapper->setNoTitleBar(true);
                    m_maskSurfaceWrapper->setNoCornerRadius(true);
                    m_maskSurfaceWrapper->setNoDecoration(true);
                    m_maskSurfaceWrapper->disableWindowAnimation();
                    m_maskSurfaceWrapper->setPositionAutomatic(false);
                }
                m_maskShellSurface = surfaceItem->shellSurface();
            }
        }
        auto childItems = node->childItems();
        for (const auto &child : std::as_const(childItems)) {
            nodes.enqueue(child);
        }
    }
}

void CaptureManagerV1::handleContextBeforeDestroy(CaptureContextV1 *context)
{
    clearContextInSelection(context);
}

CaptureContextModel::CaptureContextModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CaptureContextModel::rowCount([[maybe_unused]] const QModelIndex &parent) const
{
    return m_captureContexts.size();
}

QVariant CaptureContextModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_captureContexts.size())
        return {};
    switch (role) {
    case ContextRole:
        return QVariant::fromValue(m_captureContexts.at(index.row()));
    }
    return {};
}

QHash<int, QByteArray> CaptureContextModel::roleNames() const
{
    return QHash<int, QByteArray>{ { ContextRole, QByteArrayLiteral("context") } };
}

void CaptureContextModel::addContext(CaptureContextV1 *context)
{
    beginInsertRows(QModelIndex(), m_captureContexts.size(), m_captureContexts.size() + 1);
    m_captureContexts.push_back(context);
    endInsertRows();
}

void CaptureContextModel::removeContext(CaptureContextV1 *context)
{
    auto index = m_captureContexts.indexOf(context);
    beginRemoveRows(QModelIndex(), index, index);
    m_captureContexts.remove(index);
    endRemoveRows();
}

CaptureSourceSelector::CaptureSourceSelector(QQuickItem *parent)
    : SurfaceContainer(parent)
    , m_internalContentItem(new QQuickItem(this))
    , m_itemSelector(new ItemSelector(m_internalContentItem))
    , m_canvasContainer(new SurfaceContainer(this))
    , m_toolBarModel(new ToolBarModel(this))
{
    QQuickItemPrivate::get(m_internalContentItem)->anchors()->setFill(this);
    m_internalContentItem->setZ(1);
    QQuickItemPrivate::get(m_canvasContainer)->anchors()->setFill(this);
    m_canvasContainer->setZ(2);
    updateCursorShape();
    setAcceptedMouseButtons(Qt::LeftButton);
    setActiveFocusOnTab(false);
    connect(m_itemSelector,
            &ItemSelector::hoveredItemChanged,
            this,
            &CaptureSourceSelector::hoveredItemChanged,
            Qt::UniqueConnection);
    connect(m_itemSelector,
            &ItemSelector::selectionRegionChanged,
            this,
            &CaptureSourceSelector::handleItemSelectorSelectionRegionChanged,
            Qt::UniqueConnection);
    m_itemSelector->addCustomFilter([this](QQuickItem *item,
                                           [[maybe_unused]] ItemSelector::ItemTypes selectionHint) -> bool {
        if (auto surfaceItemContent = qobject_cast<WSurfaceItemContent *>(item)) {
            return surfaceItemContent->surface() != captureManager()->contextInSelection()->mask();
        } else if (auto surfaceItem = qobject_cast<WSurfaceItem *>(item)) {
            auto layerSurface = qobject_cast<WLayerSurface *>(surfaceItem->shellSurface());
            if (layerSurface) {
                if (QString(layerSurface->handle()->handle()->scope) == "dde-shell/desktop") {
                    return false;
                }
            }
            return surfaceItem->surface() != captureManager()->contextInSelection()->mask();
        } else {
            return true;
        }
    });
}

CaptureSourceSelector::~CaptureSourceSelector() { }

void CaptureSourceSelector::doneSelection()
{
    // Selection is done, begin to construct selection source
    connect(renderWindow(),
            &WOutputRenderWindow::renderEnd,
            this,
            &CaptureSourceSelector::createImage);
    m_internalContentItem->setVisible(false);
    m_canvas->surfaceItem()->setSubsurfacesVisible(false);
}

void CaptureSourceSelector::cancelSelection()
{
    if (captureManager() && captureManager()->contextInSelection()) {
        releaseMaskSurface();
        captureManager()->contextInSelection()->sendSourceFailed(CaptureContextV1::UserCancel);
        captureManager()->clearContextInSelection(captureManager()->contextInSelection());
    }
}

void CaptureSourceSelector::updateCursorShape()
{
    if (m_selectionMode == SelectionMode::SelectOutput
        || m_selectionMode == SelectionMode::SelectWindow) {
        setCursor(Qt::PointingHandCursor);
    } else if (m_selectionMode == SelectionMode::SelectRegion) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

wl_global *CaptureManagerV1::global() const
{
    return m_manager->global;
}

QQuickItem *CaptureSourceSelector::hoveredItem() const
{
    return m_itemSelector->hoveredItem();
}

bool CaptureSourceSelector::itemSelectionMode() const
{
    return m_itemSelectionMode;
}

void CaptureSourceSelector::setItemSelectionMode(bool itemSelection)
{
    if (m_itemSelectionMode == itemSelection)
        return;
    m_itemSelectionMode = itemSelection;
    if (m_itemSelectionMode) {
        // Let item selector do the job
        connect(m_itemSelector,
                &ItemSelector::selectionRegionChanged,
                this,
                &CaptureSourceSelector::handleItemSelectorSelectionRegionChanged,
                Qt::UniqueConnection);
    } else {
        disconnect(m_itemSelector,
                   &ItemSelector::selectionRegionChanged,
                   this,
                   &CaptureSourceSelector::handleItemSelectorSelectionRegionChanged);
    }
    updateItemSelectorItemTypes();
}

void CaptureSourceSelector::updateItemSelectorItemTypes()
{
    if (m_itemSelectionMode) {
        m_itemSelector->setSelectionTypeHint(selectionModeToItemTypes(m_selectionMode));
    } else {
        // Always item select output in non-item selection mode
        m_itemSelector->setSelectionTypeHint(ItemSelector::Output);
    }
}

WOutputRenderWindow *CaptureSourceSelector::renderWindow() const
{
    return qobject_cast<WOutputRenderWindow *>(window());
}

void CaptureSourceSelector::createImage()
{
    disconnect(renderWindow(),
               &WOutputRenderWindow::renderEnd,
               this,
               &CaptureSourceSelector::createImage);
    if (m_selectedSource) {
        m_selectedSource->createImage();
        if (m_selectedSource->imageValid()) {
            releaseMaskSurface();
        } else {
            connect(m_selectedSource,
                    &CaptureSource::imageReady,
                    this,
                    &CaptureSourceSelector::releaseMaskSurface);
        }
    } else {
        releaseMaskSurface();
    }
    captureManager()->clearContextInSelection(captureManager()->contextInSelection());
}

CaptureManagerV1 *CaptureSourceSelector::captureManager() const
{
    return m_captureManager;
}

void CaptureSourceSelector::setCaptureManager(CaptureManagerV1 *newCaptureManager)
{
    if (m_captureManager == newCaptureManager)
        return;
    m_captureManager = newCaptureManager;
    if (captureSourceHint().toInt() == 0 || captureSourceHint().testFlag(CaptureSource::Region)) {
        // Default case, no hint means all hint internally.
        doSetSelectionMode(SelectionMode::SelectRegion);
    } else if (captureSourceHint().testAnyFlags(
                   { CaptureSource::Window | CaptureSource::Surface })) {
        doSetSelectionMode(SelectionMode::SelectWindow);
    } else {
        doSetSelectionMode(SelectionMode::SelectOutput);
    }
    m_captureManager->setSelector(this);
    m_toolBarModel->updateModel();
    Q_ASSERT(captureManager()->contextInSelection());
    connect(captureManager()->contextInSelection(),
            &CaptureContextV1::finishSelect,
            this,
            &CaptureSourceSelector::doneSelection,
            Qt::UniqueConnection);
    Q_EMIT captureManagerChanged();
}

void CaptureSourceSelector::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selectionMode == SelectionMode::SelectRegion) {
        auto pos = event->position();
        auto distance = pos - m_selectionAnchor;
        if (distance.manhattanLength() > 2) {
            setItemSelectionMode(false);
            m_doNotFinish = true;
            QRectF newRegion;
            newRegion.setLeft(qMin(m_selectionAnchor.x(), pos.x()));
            newRegion.setRight(qMax(m_selectionAnchor.x(), pos.x()));
            newRegion.setTop(qMin(m_selectionAnchor.y(), pos.y()));
            newRegion.setBottom(qMax(m_selectionAnchor.y(), pos.y()));
            setSelectionRegion(newRegion);
        }
    }
}

static inline WSurfaceItemContent *findItemContent(QQuickItem *item)
{
    QQueue<QQuickItem *> q;
    q.enqueue(item);
    while (!q.empty()) {
        auto node = q.dequeue();
        if (auto content = qobject_cast<WSurfaceItemContent *>(node))
            return content;
        q.append(node->childItems());
    }
    return nullptr;
}

CaptureSourceSurface::CaptureSourceSurface(WSurfaceItemContent *surfaceItemContent,
                                           qreal devicePixelRatio)
    : CaptureSource(surfaceItemContent, devicePixelRatio, nullptr)
    , m_surfaceItemContent(surfaceItemContent)
{
}

qw_buffer *CaptureSourceSurface::internalBuffer()
{
    Q_ASSERT(m_sourceList.size() == 1);
    if (m_sourceList.first().first && m_surfaceItemContent->surface()
        && m_surfaceItemContent->surface()->buffer()) {
        if (auto clientBuffer = wlr_client_buffer_get(*m_surfaceItemContent->surface()->buffer())) {
            return qw_buffer::from(clientBuffer->source);
        } else {
            return m_surfaceItemContent->surface()->buffer();
        }
    } else {
        qWarning() << "The first source has been invalid";
        return nullptr;
    }
}

CaptureSource::CaptureSourceType CaptureSourceSurface::sourceType()
{
    return CaptureSource::Surface;
}

QRect CaptureSourceSurface::cropRect() const
{
    return m_surfaceItemContent
        ? scaledRect(m_surfaceItemContent->boundingRect(), m_devicePixelRatio).toRect()
        : QRect{};
}

QSize CaptureSourceSurface::sourceSize() const
{
    return m_surfaceItemContent ? (m_surfaceItemContent->size() * m_devicePixelRatio).toSize()
                                : QSize{};
}

CaptureSource *CaptureSourceSelector::selectedSource() const
{
    return m_selectedSource;
}

void CaptureSourceSelector::setSelectedSource(CaptureSource *newSelectedSource, const QRect &region)
{
    if (m_selectedSource == newSelectedSource)
        return;
    qCDebug(qLcCapture()) << "Set selected source to" << newSelectedSource;
    m_selectedSource = newSelectedSource;
    if (m_selectedSource) {
        m_captureManager->contextInSelection()->setSource(m_selectedSource, region);
    }
    Q_EMIT selectedSourceChanged();
}

QDebug operator<<(QDebug debug, CaptureSource &captureSource)
{
    debug << "CaptureSource(" << captureSource.sourceType() << "," << &captureSource << ")";
    return debug;
}

void CaptureSourceSelector::componentComplete()
{
    // Notify mask size now
    if (captureManager()->maskShellSurface() && captureManager()->maskSurfaceWrapper()) {
        m_canvas = captureManager()->maskSurfaceWrapper();
        m_captureManager->maskShellSurface()->resize(size().toSize());
        if (m_captureManager->maskSurfaceWrapper()->container()) {
            m_savedContainer = m_captureManager->maskSurfaceWrapper()->container();
            m_captureManager->maskSurfaceWrapper()->container()->removeSurface(
                m_captureManager->maskSurfaceWrapper());
        }
        m_canvasContainer->addSurface(m_captureManager->maskSurfaceWrapper());
        m_canvas->setX(0);
        m_canvas->setY(0);
        m_captureManager->maskSurfaceWrapper()->setWorkspaceId(
            Workspace::ShowOnAllWorkspaceId); // TODO: use a more reasonable id
    }
    QQuickItem::componentComplete();
}

void CaptureSourceSelector::mousePressEvent(QMouseEvent *event)
{
    // Only handle pressed event in SelectRegion selection.
    if (selectionMode() == SelectionMode::SelectRegion && event->button() == Qt::LeftButton) {
        m_selectionAnchor = event->position();
    }
}

void CaptureSourceSelector::mouseReleaseEvent([[maybe_unused]] QMouseEvent *event)
{
    switch (selectionMode()) {
    case SelectionMode::SelectRegion: {
        auto viewport =
            m_itemSelector->outputItem()->property("screenViewport").value<WOutputViewport *>();
        if (viewport) {
            setSelectedSource(
                new CaptureSourceRegion(viewport,
                                        mapRectToItem(viewport, selectionRegion()).toRect()),
                selectionRegion().toRect());
        }
        // Exit item selection mode after first click
        setItemSelectionMode(false);
        break;
    }
    case SelectionMode::SelectWindow: {
        if (auto surfaceItemContent = qobject_cast<WSurfaceItemContent *>(hoveredItem())) {
            setSelectedSource(
                new CaptureSourceSurface(surfaceItemContent,
                                         m_itemSelector->outputItem()->devicePixelRatio()),
                selectionRegion().toRect());
        }
        break;
    }
    case SelectionMode::SelectOutput: {
        if (auto outputItem = qobject_cast<WOutputItem *>(hoveredItem())) {
            auto viewport = outputItem->property("screenViewport").value<WOutputViewport *>();
            if (viewport) {
                setSelectedSource(new CaptureSourceOutput(viewport), selectionRegion().toRect());
            }
        }
        break;
    }
    }
}

QRectF CaptureSourceSelector::selectionRegion() const
{
    return m_selectionRegion;
}

void CaptureSourceSelector::setSelectionRegion(const QRectF &newSelectionRegion)
{
    if (m_selectionRegion == newSelectionRegion)
        return;
    m_selectionRegion = newSelectionRegion;
    Q_EMIT selectionRegionChanged();
}

void CaptureSourceSelector::handleItemSelectorSelectionRegionChanged()
{
    setSelectionRegion(m_itemSelector->selectionRegion());
}

bool CaptureSource::imageValid() const
{
    return !m_image.isNull();
}

QImage CaptureSource::image() const
{
    return m_image;
}

void CaptureSource::createImage()
{
    if (m_sourceList.size() == 1 && m_sourceList.first().first) {
        auto grabber = new WTextureCapturer(m_sourceList.first().second, this);
        grabber->grabToImage()
            .then([this](QImage image) {
                m_image = std::move(image);
                Q_EMIT imageReady();
            })
            .onFailed([](const std::exception &e) {
                qCCritical(qLcCapture) << e.what();
            });
    } else {
        // TODO: support multiple sources
    }
}

qw_buffer *CaptureSource::sourceDMABuffer()
{
    auto buffer = internalBuffer();
    if (!m_bufferConn)
        m_bufferConn =
            connect(buffer, &qw_buffer::destroyed, this, &CaptureSource::bufferDestroyed);
    return buffer;
}

void CaptureSource::copyBuffer(qw_buffer *buffer)
{
    Q_ASSERT(imageValid());
    auto width = cropRect().width();
    auto height = cropRect().height();
    uint32_t format;
    size_t stride;
    void *data;
    buffer->begin_data_ptr_access(WLR_BUFFER_DATA_PTR_ACCESS_WRITE, &data, &format, &stride);
    Q_ASSERT(stride == static_cast<size_t>(width) * 4); // For QImage
    QImage img = image().copy(cropRect());
    auto bufFormat = WTools::toImageFormat(format);
    if (image().format() != bufFormat) {
        img = image().convertToFormat(bufFormat);
    }
    memcpy(data, img.constBits(), stride * height);
    buffer->end_data_ptr_access();
}

CaptureSourceOutput::CaptureSourceOutput(WOutputViewport *viewport)
    : CaptureSource(viewport, viewport->devicePixelRatio(), nullptr)
    , m_outputViewport(viewport)
{
}

qw_buffer *CaptureSourceOutput::internalBuffer()
{
    Q_ASSERT(m_sourceList.size() == 1);
    if (m_sourceList.first().first && m_outputViewport->wTextureProvider())
        return m_outputViewport->wTextureProvider()->qwBuffer();
    else
        return nullptr;
}

QRect CaptureSourceOutput::cropRect() const
{
    return m_outputViewport
        ? scaledRect(m_outputViewport->boundingRect(), m_outputViewport->devicePixelRatio())
              .toRect()
        : QRect{};
}

QSize CaptureSourceOutput::sourceSize() const
{
    return m_outputViewport ? (m_outputViewport->size() * m_devicePixelRatio).toSize() : QSize{};
}

CaptureSource::CaptureSourceType CaptureSourceOutput::sourceType()
{
    return CaptureSource::Output;
}

CaptureSourceRegion::CaptureSourceRegion(WOutputViewport *viewport, const QRect &region)
    : CaptureSource(viewport, viewport->devicePixelRatio(), nullptr)
{
    m_viewportRegions.push_back({ viewport, region });
}

qw_buffer *CaptureSourceRegion::internalBuffer()
{
    if (m_sourceList.size() == 1 && m_sourceList.first().first
        && m_sourceList.first().second->wTextureProvider()) {
        return m_sourceList.first().second->wTextureProvider()->qwBuffer();
    } else {
        return nullptr;
    }
}

CaptureSource::CaptureSourceType CaptureSourceRegion::sourceType()
{
    return CaptureSource::Region;
}

QRect CaptureSourceRegion::cropRect() const
{
    QRect result{};
    WOutputViewport *ref = nullptr;
    for (const auto &[viewport, region] : std::as_const(m_viewportRegions)) {
        if (!viewport)
            continue;
        if (!ref) {
            result = scaledRect(region, viewport->devicePixelRatio()).toRect();
            ref = viewport;
        } else {
            auto mapped =
                scaledRect(ref->mapRectFromItem(viewport, region), viewport->devicePixelRatio())
                    .toRect(); // FIXME: firstly map or firstly scale?
            result = result.united(mapped);
        }
    }
    return result;
}

QSize CaptureSourceRegion::sourceSize() const
{
    QRect result{};
    WOutputViewport *ref = nullptr;
    for (const auto &[viewport, region] : std::as_const(m_viewportRegions)) {
        if (!viewport)
            continue;
        if (!ref) {
            result = scaledRect(viewport->boundingRect(), viewport->devicePixelRatio()).toRect();
            ref = viewport;
        } else {
            auto mapped = ref->mapRectFromItem(
                                 viewport,
                                 scaledRect(viewport->boundingRect(), viewport->devicePixelRatio()))
                              .toRect(); // FIXME: firstly map or firstly scale?
            result = result.united(mapped);
        }
    }
    return result.size();
}

bool CaptureSourceRegion::addViewportRegion(WOutputViewport *viewport, const QRect &region)
{
    auto scenePos = viewport->mapToScene({ 0, 0 });
    int insertIndex;
    for (insertIndex = 0; insertIndex < m_viewportRegions.size(); ++insertIndex) {
        auto &[v, _] = m_viewportRegions[insertIndex];
        auto vScenePos = v->mapToScene({ 0, 0 });
        if (vScenePos.x() > scenePos.x()) {
            break;
        }
    }
    m_viewportRegions.insert(insertIndex, { viewport, region });
    return true;
}

void CaptureSourceSelector::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (m_captureManager->maskShellSurface()) {
        m_captureManager->maskShellSurface()->resize(newGeometry.size().toSize());
    }
    SurfaceContainer::geometryChange(newGeometry, oldGeometry);
}

CaptureSourceSelector::SelectionMode CaptureSourceSelector::selectionMode() const
{
    return m_selectionMode;
}

void CaptureSourceSelector::setSelectionMode(const SelectionMode &newSelectionMode)
{
    if (captureSourceHint().toInt() == 0
        || captureSourceHint().testAnyFlags(selectionModeHint(newSelectionMode))) {
        doSetSelectionMode(newSelectionMode);
    } else {
        qCWarning(qLcCapture()) << "Trying to set selection mode not support, discarded.";
    }
}

void CaptureSourceSelector::doSetSelectionMode(const SelectionMode &newSelectionMode)
{
    if (m_selectionMode == newSelectionMode)
        return;
    m_selectionMode = newSelectionMode;
    updateCursorShape();
    setItemSelectionMode(true);
    updateItemSelectorItemTypes();
    Q_EMIT selectionModeChanged();
}

CaptureSource::CaptureSourceHint CaptureSourceSelector::selectionModeHint(
    const SelectionMode &selectionMode)
{
    switch (selectionMode) {
    case SelectionMode::SelectOutput:
        return CaptureSource::Output;
    case SelectionMode::SelectRegion:
        return CaptureSource::Region;
    case SelectionMode::SelectWindow:
        return { CaptureSource::Window | CaptureSource::Surface };
    default:
        Q_UNREACHABLE();
    }
}

ItemSelector::ItemTypes CaptureSourceSelector::selectionModeToItemTypes(
    const SelectionMode &selectionMode) const
{
    switch (selectionMode) {
    case SelectionMode::SelectOutput:
        return ItemSelector::Output;
    case SelectionMode::SelectRegion:
        return ItemSelector::Output | ItemSelector::Window | ItemSelector::Surface;
    case SelectionMode::SelectWindow:
        return ItemSelector::Surface;
    default:
        Q_UNREACHABLE();
    }
}

QQmlListProperty<QObject> CaptureSourceSelector::contents() const
{
    return QQuickItemPrivate::get(m_internalContentItem)->data();
}

void CaptureSourceSelector::itemChange(ItemChange change, const ItemChangeData &data)
{
    switch (change) {
    case QQuickItem::ItemChange::ItemParentHasChanged:
        Q_ASSERT_X(parentContainer(),
                   __func__,
                   "CaptureSourceSelector must be attached to a SurfaceContainer.");
        ensureQmlContext();
        break;
    default:
        break;
    }

    SurfaceContainer::itemChange(change, data);
}

ToolBarModel::ToolBarModel(CaptureSourceSelector *selector)
    : QAbstractListModel(selector)
{
    updateModel();
}

void ToolBarModel::updateModel()
{
    beginResetModel();
    m_data.clear();
    auto sourceHint = selector()->captureSourceHint();
    if (sourceHint.testFlag(CaptureSource::Region)) {
        m_data.push_back({ "select_region", CaptureSourceSelector::SelectionMode::SelectRegion });
    }
    if (sourceHint.testAnyFlags({ CaptureSource::Surface | CaptureSource::Window })) {
        m_data.push_back({ "select_window", CaptureSourceSelector::SelectionMode::SelectWindow });
    }
    if (sourceHint.testFlag(CaptureSource::Output)) {
        m_data.push_back({ "select_output", CaptureSourceSelector::SelectionMode::SelectOutput });
    }
    Q_EMIT countChanged();
    endResetModel();
}

CaptureSourceSelector *ToolBarModel::selector() const
{
    return static_cast<CaptureSourceSelector *>(parent());
}

int ToolBarModel::rowCount(const QModelIndex &) const
{
    return m_data.size();
}

QVariant ToolBarModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= rowCount())
        return {};
    switch (role) {
    case IconNameRole:
        return QVariant::fromValue(m_data[index.row()].first);
    case SelectionModeRole:
        return QVariant::fromValue(m_data[index.row()].second);
    default:
        return {};
    }
}

QHash<int, QByteArray> ToolBarModel::roleNames() const
{
    return { { IconNameRole, "iconName" }, { SelectionModeRole, "selectionMode" } };
}

ToolBarModel *CaptureSourceSelector::toolBarModel() const
{
    return m_toolBarModel;
}

void CaptureSourceSelector::releaseMaskSurface()
{
    // Mask surface should be reparented before destruction.
    // If reparent in destructor, it's already marked as deleted in qml
    disconnect(m_selectedSource,
               &CaptureSource::imageReady,
               this,
               &CaptureSourceSelector::releaseMaskSurface);
    if (m_savedContainer) {
        QQueue<WWrapPointer<SurfaceWrapper>> q;
        q.enqueue(m_canvas);
        while (!q.isEmpty()) {
            auto node = q.dequeue();
            if (node) {
                m_canvasContainer->removeSurface(node);
                node->setWorkspaceId(-1);
                m_savedContainer->addSurface(node);
                for (const auto &child : std::as_const(node->subSurfaces())) {
                    q.enqueue(child);
                }
            }
        }
    }
}
