// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "capture.h"

#include "impl/capturev1impl.h"

#include <woutputitem.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wquicktextureproxy.h>
#include <wsgtextureprovider.h>
#include <wtools.h>

#include <qwcompositor.h>
#include <qwdisplay.h>

#include <QLoggingCategory>
#include <QQueue>
#include <QQuickItemGrabResult>
#include <QSGTextureProvider>

extern "C" {
#include <wlr/types/wlr_compositor.h>
}

Q_LOGGING_CATEGORY(qLcCapture, "treeland.capture")
#define DECORATION "treeland_decoration"
#define TITLEBAR "treeland_decoration_titlebar"

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

void CaptureContextV1::setSource(CaptureSource *source)
{
    if (m_captureSource == source)
        return;
    m_captureSource = source;
    m_handle->sendSourceReady(source->captureRegion(),
                              captureSourceTypeToProtocol(source->sourceType()));
    Q_EMIT sourceChanged();
    Q_EMIT finishSelect();
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
    return CaptureSource::CaptureSourceHint(m_handle->sourceHint);
}

CaptureContextV1::CaptureContextV1(treeland_capture_context_v1 *h, QObject *parent)
    : QObject(parent)
    , m_handle(h)
{
    connect(h, &treeland_capture_context_v1::selectSource, this, &CaptureContextV1::onSelectSource);
    connect(h, &treeland_capture_context_v1::capture, this, &CaptureContextV1::onCapture);
}

void CaptureContextV1::onSelectSource()
{
    treeland_capture_context_v1 *context = qobject_cast<treeland_capture_context_v1 *>(sender());
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
            m_captureSource->captureRegion().width(),
            m_captureSource->captureRegion().height(),
            m_captureSource->captureRegion().width() * 4);
        m_frame->sendBufferDone();
        connect(m_frame,
                &treeland_capture_frame_v1::copy,
                this,
                &CaptureContextV1::handleFrameCopy);
        connect(m_frame, &treeland_capture_frame_v1::beforeDestroy, this, [this] {
            m_frame = nullptr;
        });
    };
    if (m_captureSource->valid()) {
        notifyBuffer();
    } else {
        connect(m_captureSource, &CaptureSource::ready, this, notifyBuffer);
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
    Q_EMIT finishSelect();
}

CaptureManagerV1::CaptureManagerV1(QObject *parent)
    : QObject(parent)
    , m_manager(nullptr)
    , m_captureContextModel(new CaptureContextModel(this))
    , m_contextInSelection(nullptr)
{
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

void CaptureManagerV1::create(WServer *server)
{
    m_manager = new treeland_capture_manager_v1(server->handle()->handle(), this);
    connect(m_manager,
            &treeland_capture_manager_v1::newCaptureContext,
            this,
            [this](treeland_capture_context_v1 *context) {
                auto quickContext = new CaptureContextV1(context, this);
                m_captureContextModel->addContext(quickContext);
                connect(context,
                        &treeland_capture_context_v1::beforeDestroy,
                        quickContext,
                        [this, quickContext] {
                            m_captureContextModel->removeContext(quickContext);
                            quickContext->deleteLater();
                        });
                connect(quickContext,
                        &CaptureContextV1::selectInfoReady,
                        this,
                        &CaptureManagerV1::onCaptureContextSelectSource);
            });
}

void CaptureManagerV1::destroy(WServer *server)
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
    connect(context, &CaptureContextV1::destroyed, this, [this, context] {
        clearContextInSelection(context);
    });
    connect(context, &CaptureContextV1::finishSelect, this, [this, context] {
        clearContextInSelection(context);
    });
    m_contextInSelection = context;
    if (context->freeze()) {
        toggleFreezeAllSurfaceItems(true);
    }
    Q_EMIT contextInSelectionChanged();
}

void CaptureManagerV1::clearContextInSelection(CaptureContextV1 *context)
{
    if (m_contextInSelection == context) {
        if (m_contextInSelection->freeze()) {
            toggleFreezeAllSurfaceItems(false);
        }
        m_contextInSelection = nullptr;
        Q_EMIT contextInSelectionChanged();
    }
}

void CaptureManagerV1::toggleFreezeAllSurfaceItems(bool freeze)
{
    Q_ASSERT(m_outputRenderWindow);
    QQueue<QObject *> nodes;
    nodes.enqueue(m_outputRenderWindow);
    while (!nodes.isEmpty()) {
        auto node = nodes.dequeue();
        if (auto content = qobject_cast<WSurfaceItemContent *>(node)) {
            content->setLive(!freeze);
        }
        for (auto child : node->children()) {
            nodes.enqueue(child);
        }
    }
}

CaptureContextModel::CaptureContextModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CaptureContextModel::rowCount(const QModelIndex &parent) const
{
    return m_captureContexts.size();
}

QVariant CaptureContextModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_captureContexts.size())
        return QVariant();
    switch (role) {
    case ContextRole:
        return QVariant::fromValue(m_captureContexts.at(index.row()));
    }
    return QVariant();
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
    : QQuickItem(parent)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setKeepMouseGrab(true);
    setActiveFocusOnTab(false);
    setCursor(Qt::CrossCursor);
}

void CaptureSourceSelector::doneSelection()
{
    // Selection is done, begin to construct selection source
    setVisible(false);
    renderWindow()->render(); // Flush frame to hide capture selector mask
    if (itemSelectionMode()) {
        if (auto surfaceItem = qobject_cast<WSurfaceItem *>(hoveredItem())) {
            setSelectedSource(new CaptureSourceSurface(surfaceItem));
        } else if (auto outputItem = qobject_cast<WOutputItem *>(hoveredItem())) {
            auto viewport = outputItem->property("onscreenViewport").value<WOutputViewport *>();
            if (viewport) {
                setSelectedSource(new CaptureSourceOutput(viewport));
            }
        }
    } else {
        auto outputItem = qobject_cast<WOutputItem *>(hoveredItem());
        auto viewport = outputItem->property("onscreenViewport").value<WOutputViewport *>();
        if (viewport) {
            setSelectedSource(
                new CaptureSourceRegion(viewport,
                                        mapRectToItem(viewport, selectionRegion()).toRect()));
        }
    }
}

wl_global *CaptureManagerV1::global() const
{
    return m_manager->global;
}

void CaptureSourceSelector::hoverMoveEvent(QHoverEvent *event)
{
    checkHoveredItem(event->position());
}

QQuickItem *CaptureSourceSelector::hoveredItem() const
{
    return m_hoveredItem;
}

void CaptureSourceSelector::setHoveredItem(QQuickItem *newHoveredItem)
{
    if (m_hoveredItem == newHoveredItem)
        return;
    m_hoveredItem = newHoveredItem;
    Q_EMIT hoveredItemChanged();
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
    Q_EMIT itemSelectionModeChanged();
}

void CaptureSourceSelector::checkHoveredItem(QPointF pos)
{
    for (auto it = m_selectableItems.crbegin(); it != m_selectableItems.crend(); it++) {
        if (!itemSelectionMode() && !qobject_cast<WOutputItem *>(*it))
            continue;
        auto itemRect = (*it)->mapRectToItem(this, (*it)->boundingRect());
        auto detectionRect = itemRect;
        if (qobject_cast<WSurfaceItem *>((*it)->parentItem())
            && (*it)->objectName() == DECORATION) {
            for (const auto &child : (*it)->childItems()) {
                if (child->objectName() == TITLEBAR) {
                    detectionRect = child->mapRectToItem(this, child->boundingRect());
                }
            }
        } else if (auto surfaceItem = qobject_cast<WSurfaceItem *>(*it)) {
            if (surfaceItem->contentItem()) {
                detectionRect = surfaceItem->contentItem()->mapRectToItem(
                    this,
                    surfaceItem->contentItem()->boundingRect());
                itemRect = detectionRect;
            }
        }
        if (detectionRect.contains(pos)) {
            setHoveredItem(*it);
            setSelectionRegion(itemRect);
            break;
        }
    }
}

WOutputRenderWindow *CaptureSourceSelector::renderWindow() const
{
    return qobject_cast<WOutputRenderWindow *>(window());
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
    Q_EMIT captureManagerChanged();
}

void CaptureSourceSelector::mouseMoveEvent(QMouseEvent *event)
{
    auto pos = event->position();
    auto distance = pos - m_selectionAnchor;
    if (distance.manhattanLength() > 2) {
        setItemSelectionMode(false);
        QRect newRegion;
        newRegion.setLeft(qMin(m_selectionAnchor.x(), pos.x()));
        newRegion.setRight(qMax(m_selectionAnchor.x(), pos.x()));
        newRegion.setTop(qMin(m_selectionAnchor.y(), pos.y()));
        newRegion.setBottom(qMax(m_selectionAnchor.y(), pos.y()));
        setSelectionRegion(newRegion);
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

CaptureSourceSurface::CaptureSourceSurface(WSurfaceItem *surfaceItem)
    : CaptureSource(findItemContent(surfaceItem->contentItem()), nullptr)
    , m_surfaceItem(surfaceItem)
{
}

qw_buffer *CaptureSourceSurface::sourceDMABuffer()
{
    // TODO Get correct DMA buffer
    return nullptr;
}

QRect CaptureSourceSurface::captureRegion()
{
    return m_surfaceItem->contentItem()->boundingRect().toRect();
}

CaptureSource::CaptureSourceType CaptureSourceSurface::sourceType()
{
    return CaptureSource::Surface;
}

CaptureSource *CaptureSourceSelector::selectedSource() const
{
    return m_selectedSource;
}

void CaptureSourceSelector::setSelectedSource(CaptureSource *newSelectedSource)
{
    if (m_selectedSource == newSelectedSource)
        return;
    qCDebug(qLcCapture()) << "Set selected source to" << newSelectedSource;
    m_selectedSource = newSelectedSource;
    if (m_selectedSource) {
        m_captureManager->contextInSelection()->setSource(m_selectedSource);
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
    m_captureManager = qmlEngine(this)->singletonInstance<CaptureManagerV1 *>("Treeland.Protocols",
                                                                              "CaptureManagerV1");
    Q_ASSERT(window());
    auto renderWindow = qobject_cast<WOutputRenderWindow *>(window());
    m_selectableItems = WOutputRenderWindow::paintOrderItemList(
        renderWindow->contentItem(),
        [this](QQuickItem *item) {
            auto context = m_captureManager->contextInSelection();
            auto sourceHint = context->sourceHint();
            if (auto viewport = qobject_cast<WOutputItem *>(item)
                    && sourceHint.testFlag(CaptureSource::Output)) {
                return true;
            } else if (auto surfaceItem = qobject_cast<WSurfaceItem *>(item)
                           && sourceHint.testFlag(CaptureSource::Window)) {
                return true;
            } else if (qobject_cast<WSurfaceItem *>(item->parentItem())
                       && item->objectName() == DECORATION
                       && sourceHint.testFlag(CaptureSource::Window)) {
                return true;
            } else {
                return false;
            }
        });
    QQuickItem::componentComplete();
}

void CaptureSourceSelector::mousePressEvent(QMouseEvent *event)
{
    switch (event->button()) {
    case Qt::LeftButton: {
        m_selectionAnchor = event->position();
        break;
    }
    default:
        break;
    }
}

void CaptureSourceSelector::mouseReleaseEvent(QMouseEvent *event)
{
    doneSelection();
}

void CaptureSourceSelector::releaseResources() { }

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

CaptureSource::CaptureSource(WTextureProviderProvider *textureProvider, QObject *parent)
    : QObject(parent)
    , m_provider(textureProvider)
{
    auto grabber = new WTextureCapturer(m_provider, this);
    grabber->grabToImage()
        .then([this](QImage image) {
            m_image = image;
            Q_EMIT ready();
        })
        .onFailed([](const std::exception &e) {
            qCCritical(qLcCapture) << e.what();
        });
}

bool CaptureSource::valid() const
{
    return !m_image.isNull();
}

QImage CaptureSource::image() const
{
    return m_image;
}

void CaptureSource::copyBuffer(qw_buffer *buffer)
{
    Q_ASSERT(valid());
    auto width = captureRegion().width();
    auto height = captureRegion().height();
    uint32_t format;
    size_t stride;
    void *data;
    buffer->begin_data_ptr_access(WLR_BUFFER_DATA_PTR_ACCESS_WRITE, &data, &format, &stride);
    Q_ASSERT(stride == width * 4);
    QImage img = image().copy(captureRegion());
    auto bufFormat = WTools::toImageFormat(format);
    qDebug() << WTools::toImageFormat(format) << image().format();
    if (image().format() != bufFormat) {
        img = image().convertToFormat(bufFormat);
    }
    memcpy(data, img.constBits(), stride * height);
    buffer->end_data_ptr_access();
}

CaptureSourceOutput::CaptureSourceOutput(WOutputViewport *viewport)
    : CaptureSource(viewport, nullptr)
    , m_outputViewport(viewport)
{
}

qw_buffer *CaptureSourceOutput::sourceDMABuffer()
{
    // TODO Get correct DMA buffer
    return nullptr;
}

QRect CaptureSourceOutput::captureRegion()
{
    return m_outputViewport->boundingRect().toRect();
}

CaptureSource::CaptureSourceType CaptureSourceOutput::sourceType()
{
    return CaptureSource::Output;
}

CaptureSourceRegion::CaptureSourceRegion(WOutputViewport *viewport, const QRect &region)
    : CaptureSource(viewport, nullptr)
    , m_outputViewport(viewport)
    , m_region(region)
{
}

qw_buffer *CaptureSourceRegion::sourceDMABuffer()
{
    // TODO Get correct DMA buffer
    return nullptr;
}

QRect CaptureSourceRegion::captureRegion()
{
    return m_region;
}

CaptureSource::CaptureSourceType CaptureSourceRegion::sourceType()
{
    return CaptureSource::Region;
}
