// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "rootsurfacecontainer.h"

#include "common/treelandlogging.h"
#include "output/output.h"
#include "seat/helper.h"
#include "seat/seatsmanager.h"
#include "surface/surfacewrapper.h"

#include <wcursor.h>
#include <woutput.h>
#include <woutputitem.h>
#include <woutputlayout.h>
#include <wxdgpopupsurface.h>
#include <wxdgtoplevelsurface.h>
#include <wseat.h>
#include <winputdevice.h>

#include <qwoutputlayout.h>

#include <QQuickWindow>

WAYLIB_SERVER_USE_NAMESPACE

OutputListModel::OutputListModel(QObject *parent)
    : ObjectListModel("output", parent)
{
}

RootSurfaceContainer::RootSurfaceContainer(QQuickItem *parent)
    : SurfaceContainer(parent)
    , m_outputModel(new OutputListModel(this))
    , m_cursor(new WCursor(this))
{
    m_cursor->setEventWindow(window());
}

RootSurfaceContainer::~RootSurfaceContainer()
{
    // Clean up per-seat containers
    qDeleteAll(m_seatContainers);
    m_seatContainers.clear();
}

void RootSurfaceContainer::init(WServer *server)
{
    m_outputLayout = new WOutputLayout(server);
    m_cursor->setLayout(m_outputLayout);
    connect(m_outputLayout, &WOutputLayout::implicitWidthChanged, this, [this] {
        const auto width = m_outputLayout->implicitWidth();
        window()->setWidth(width);
        setWidth(width);
    });

    connect(m_outputLayout, &WOutputLayout::implicitHeightChanged, this, [this] {
        const auto height = m_outputLayout->implicitHeight();
        window()->setHeight(height);
        setHeight(height);
    });

    m_outputLayout->safeConnect(&qw_output_layout::notify_change, this, [this] {
        for (auto output : std::as_const(outputs())) {
            output->updatePositionFromLayout();
        }
        ensureCursorVisible();

        // for (auto s : m_surfaceContainer->surfaces()) {
        //     ensureSurfaceNormalPositionValid(s);
        //     updateSurfaceOutputs(s);
        // }
    });

    m_dragSurfaceItem = new WSurfaceItem(window()->contentItem());
    m_dragSurfaceItem->setZ(
        static_cast<std::underlying_type_t<WOutputLayout::Layer>>(WOutputLayout::Layer::Cursor)
        - 1);
    m_dragSurfaceItem->setFlags(WSurfaceItem::DontCacheLastBuffer);

    m_cursor->safeConnect(&WCursor::positionChanged, this, [this] {
        m_dragSurfaceItem->setPosition(m_cursor->position());
    });

    m_cursor->safeConnect(&WCursor::requestedDragSurfaceChanged, this, [this] {
        m_dragSurfaceItem->setSurface(m_cursor->requestedDragSurface());
    });

    setupSeatManagement();
}

SurfaceWrapper *RootSurfaceContainer::getSurface(WSurface *surface) const
{
    const auto surfaces = this->surfaces();
    for (const auto &wrapper : surfaces) {
        if (wrapper->surface() == surface)
            return wrapper;
    }
    return nullptr;
}

SurfaceWrapper *RootSurfaceContainer::getSurface(WToplevelSurface *surface) const
{
    const auto surfaces = this->surfaces();
    for (const auto &wrapper : surfaces) {
        if (wrapper->shellSurface() == surface)
            return wrapper;
    }
    return nullptr;
}

void RootSurfaceContainer::destroyForSurface(SurfaceWrapper *wrapper)
{
    // Clean up per-seat state for this surface
    for (auto *container : std::as_const(m_seatContainers)) {
        container->surfaceDestroyed(wrapper);
    }

    auto *helper = Helper::instance();
    if (helper && helper->activatedSurface() == wrapper) {
        helper->setActivatedSurface(nullptr);
    }

    wrapper->markWrapperToRemoved();
}

void RootSurfaceContainer::addOutput(Output *output)
{
    m_outputModel->addObject(output);
    m_outputLayout->autoAdd(output->output());
    if (!m_primaryOutput)
        setPrimaryOutput(output);

    // Register all outputs (including primary and proxy/copy) to ensure
    // LayerSurfaceContainer is initialized for every active display.
    SurfaceContainer::addOutput(output);
}

void RootSurfaceContainer::removeOutput(Output *output)
{
    m_outputModel->removeObject(output);
    SurfaceContainer::removeOutput(output);

    for (auto *container : std::as_const(m_seatContainers)) {
        if (container->moveResizeSurface() &&
            container->moveResizeSurface()->ownsOutput() == output) {
            container->endMoveResize();
        }
    }

    m_outputLayout->remove(output->output());
    if (m_primaryOutput == output) {
        const auto outputs = m_outputLayout->outputs();
        if (!outputs.isEmpty()) {
            auto newPrimaryOutput = Helper::instance()->getOutput(outputs.first());
            setPrimaryOutput(newPrimaryOutput);
        }
    }

    // ensure cursor within output
    const auto outputPos = output->outputItem()->position();
    if (output->geometry().contains(m_cursor->position()) && m_primaryOutput) {
        const auto posInOutput = m_cursor->position() - outputPos;
        const auto newCursorPos = m_primaryOutput->outputItem()->position() + posInOutput;

        if (m_primaryOutput->geometry().contains(newCursorPos))
            Helper::instance()->setCursorPosition(newCursorPos);
        else
            Helper::instance()->setCursorPosition(m_primaryOutput->geometry().center());
    }
}

void RootSurfaceContainer::beginMoveResize(SurfaceWrapper *surface, Qt::Edges edges)
{
    // Delegate to default seat (seats.first())
    beginMoveResizeForSeat(nullptr, surface, edges);
}

void RootSurfaceContainer::doMoveResize(const QPointF &incrementPos)
{
    // Delegate to default seat (seats.first())
    doMoveResizeForSeat(nullptr, incrementPos);
}

void RootSurfaceContainer::cancelMoveResize(SurfaceWrapper *surface)
{
    // Check all seat containers
    for (auto *container : std::as_const(m_seatContainers)) {
        if (container->moveResizeSurface() == surface) {
            container->endMoveResize();
            return;
        }
    }
}

void RootSurfaceContainer::endMoveResize()
{
    endMoveResizeForSeat(nullptr);
}

SurfaceWrapper *RootSurfaceContainer::moveResizeSurface() const
{
    return getMoveResizeSurfaceForSeat(nullptr);
}

void RootSurfaceContainer::startMove(SurfaceWrapper *surface)
{
    beginMoveResizeForSeat(nullptr, surface, Qt::Edges{});
    Helper::instance()->activateSurface(surface);
}

void RootSurfaceContainer::startResize(SurfaceWrapper *surface, Qt::Edges edges)
{
    Q_ASSERT(edges != 0);
    beginMoveResizeForSeat(nullptr, surface, edges);

    surface->shellSurface()->setResizeing(true);
    Helper::instance()->activateSurface(surface);
}

void RootSurfaceContainer::addSurface(SurfaceWrapper *)
{
    Q_UNREACHABLE_RETURN();
}

void RootSurfaceContainer::removeSurface(SurfaceWrapper *)
{
    Q_UNREACHABLE_RETURN();
}

void RootSurfaceContainer::addBySubContainer(SurfaceContainer *sub, SurfaceWrapper *surface)
{
    SurfaceContainer::addBySubContainer(sub, surface);

    if (surface->type() != SurfaceWrapper::Type::Layer) {
        // RootSurfaceContainer does not have control over layer surface's position and ownsOutput
        // All things are done in LayerSurfaceContainer

        setupSurfaceRequestHandlers(surface);

        if (!surface->ownsOutput()) {
            auto parentSurface = surface->parentSurface();
            auto output = parentSurface ? parentSurface->ownsOutput() : primaryOutput();

            if (auto xdgPopupSurface = qobject_cast<WXdgPopupSurface *>(surface->shellSurface())) {
                if (parentSurface->type() != SurfaceWrapper::Type::Layer) {
                    // If parentSurface is Layer surface, follow parentSurface->ownsOutput
                    auto pos = parentSurface->position() + parentSurface->surfaceItem()->position()
                        + xdgPopupSurface->getPopupPosition();
                    if (auto op = m_outputLayout->handle()->output_at(pos.x(), pos.y()))
                        output =
                            Helper::instance()->getOutput(WOutput::fromHandle(qw_output::from(op)));
                }
            }
            surface->setOwnsOutput(output);
        }

        connect(surface, &SurfaceWrapper::geometryChanged, this, [this, surface] {
            updateSurfaceOutputs(surface);
        });

        updateSurfaceOutputs(surface);
    }
}

void RootSurfaceContainer::removeBySubContainer(SurfaceContainer *sub, SurfaceWrapper *surface)
{
    // End move/resize for this surface in all containers
    for (auto *container : std::as_const(m_seatContainers)) {
        if (container->moveResizeSurface() == surface) {
            container->endMoveResize();
        }
    }

    SurfaceContainer::removeBySubContainer(sub, surface);
}

bool RootSurfaceContainer::filterSurfaceGeometryChanged(SurfaceWrapper *surface,
                                                        QRectF &newGeometry,
                                                        const QRectF &oldGeometry)
{
    // Helper function to adjust anchor-based position during resize
    // When resizing from an edge (e.g., right edge), the opposite position remains fixed
    auto adjustForResize = [](QRectF &geometry,
                              const QRectF &oldGeometry,
                              Qt::Edges edges) {
        if (edges & Qt::RightEdge)
            geometry.moveLeft(oldGeometry.left());
        if (edges & Qt::BottomEdge)
            geometry.moveTop(oldGeometry.top());
        if (edges & Qt::LeftEdge)
            geometry.moveRight(oldGeometry.right());
        if (edges & Qt::TopEdge)
            geometry.moveBottom(oldGeometry.bottom());
    };

    // Check all per-seat containers for move/resize state
    for (auto *container : std::as_const(m_seatContainers)) {
        auto &mrState = container->moveResizeState();
        if (mrState.surface == surface) {
            const auto &edges = mrState.edges;
            if (edges != 0 && mrState.settingPositionFlag) {
                // CRITICAL: Return true ONLY during recursive updates to prevent infinite loops
                Q_ASSERT(newGeometry.size() == oldGeometry.size());
                return true;
            }

            if (edges != 0) {
                // Adjust anchor-based position during resize
                QRectF adjustedGeo = newGeometry;
                adjustForResize(adjustedGeo, oldGeometry, edges);

                if (adjustedGeo.topLeft() != newGeometry.topLeft()) {
                    newGeometry = adjustedGeo;
                    mrState.settingPositionFlag = true;
                    QPointF alignedPos = surface->alignToPixelGrid(newGeometry.topLeft());
                    newGeometry.moveTopLeft(alignedPos);
                    surface->setPosition(alignedPos);
                    mrState.settingPositionFlag = false;
                }
                return false;
            }
        }
    }

    return false;
}

bool RootSurfaceContainer::filterSurfaceStateChange(SurfaceWrapper *surface,
                                                    [[maybe_unused]] SurfaceWrapper::State newState,
                                                    [[maybe_unused]] SurfaceWrapper::State oldState)
{
    // Check all per-seat containers for move/resize state
    for (auto *container : std::as_const(m_seatContainers)) {
        const auto &mrState = container->moveResizeState();
        if (mrState.surface == surface && mrState.edges != 0)
            return true;
    }

    return false;
}

WOutputLayout *RootSurfaceContainer::outputLayout() const
{
    return m_outputLayout;
}

WCursor *RootSurfaceContainer::cursor() const
{
    return m_cursor;
}

Output *RootSurfaceContainer::cursorOutput() const
{
    Q_ASSERT(m_cursor->layout() == m_outputLayout);
    const auto &pos = m_cursor->position();
    auto o = m_outputLayout->handle()->output_at(pos.x(), pos.y());
    if (!o)
        return nullptr;

    return Helper::instance()->getOutput(WOutput::fromHandle(qw_output::from(o)));
}

Output *RootSurfaceContainer::primaryOutput() const
{
    return m_primaryOutput;
}

void RootSurfaceContainer::setPrimaryOutput(Output *newPrimaryOutput)
{
    if (m_primaryOutput == newPrimaryOutput)
        return;
    m_primaryOutput = newPrimaryOutput;
    Q_EMIT primaryOutputChanged();
}

const QList<Output *> &RootSurfaceContainer::outputs() const
{
    return m_outputModel->objects();
}

void RootSurfaceContainer::ensureCursorVisible()
{
    const auto cursorPos = m_cursor->position();
    if (m_outputLayout->handle()->output_at(cursorPos.x(), cursorPos.y()))
        return;

    if (m_primaryOutput) {
        Helper::instance()->setCursorPosition(m_primaryOutput->geometry().center());
    }
}

void RootSurfaceContainer::updateSurfaceOutputs(SurfaceWrapper *surface)
{
    const QRectF geometry = surface->geometry();
    auto outputs = m_outputLayout->getIntersectedOutputs(geometry.toRect());
    surface->setOutputs(outputs);
    // TODO: Update ownsOutput during move/resize on multi-output systems
}

static qreal pointToRectMinDistance(const QPointF &pos, const QRectF &rect)
{
    if (rect.contains(pos))
        return 0;
    return std::min({ std::abs(rect.x() - pos.x()),
                      std::abs(rect.y() - pos.y()),
                      std::abs(rect.right() - pos.x()),
                      std::abs(rect.bottom() - pos.y()) });
}

static QRectF adjustRectToMakePointVisible(const QRectF &inputRect,
                                           const QPointF &absolutePoint,
                                           const QList<QRectF> &visibleAreas)
{
    Q_ASSERT(inputRect.contains(absolutePoint));
    QRectF adjustedRect = inputRect;

    QRectF targetRect;
    qreal distanceToTargetRect = std::numeric_limits<qreal>::max();
    for (const QRectF &area : visibleAreas) {
        Q_ASSERT(!area.isEmpty());
        if (area.contains(absolutePoint))
            return adjustedRect;
        const auto distance = pointToRectMinDistance(absolutePoint, area);
        if (distance < distanceToTargetRect) {
            distanceToTargetRect = distance;
            targetRect = area;
        }
    }
    Q_ASSERT(!targetRect.isEmpty());

    if (absolutePoint.x() < targetRect.x())
        adjustedRect.moveLeft(adjustedRect.x() + targetRect.x() - absolutePoint.x());
    else if (absolutePoint.x() > targetRect.right())
        adjustedRect.moveRight(adjustedRect.right() + targetRect.right() - absolutePoint.x());

    if (absolutePoint.y() < targetRect.y())
        adjustedRect.moveTop(adjustedRect.y() + targetRect.y() - absolutePoint.y());
    else if (absolutePoint.y() > targetRect.bottom())
        adjustedRect.moveBottom(adjustedRect.bottom() + targetRect.bottom() - absolutePoint.y());

    return adjustedRect;
}

void RootSurfaceContainer::ensureSurfaceNormalPositionValid(SurfaceWrapper *surface)
{
    if (surface->type() == SurfaceWrapper::Type::Layer)
        return;

    auto normalGeo = surface->normalGeometry();
    if (normalGeo.size().isEmpty())
        return;

    auto output = surface->ownsOutput();
    if (!output)
        return;

    QList<QRectF> outputRects;
    outputRects.reserve(outputs().size());
    for (auto o : outputs())
        outputRects << o->validGeometry();

    // Ensure window is not outside the screen
    const QPointF mustVisiblePosOfSurface(qMin(normalGeo.right(), normalGeo.x() + 20),
                                          qMin(normalGeo.bottom(), normalGeo.y() + 20));
    normalGeo = adjustRectToMakePointVisible(normalGeo, mustVisiblePosOfSurface, outputRects);

    // Ensure titlebar is not outside the screen
    const auto titlebarGeometry = surface->titlebarGeometry().translated(normalGeo.topLeft());
    if (titlebarGeometry.isValid()) {
        bool titlebarGeometryAdjusted = false;
        for (auto r : std::as_const(outputRects)) {
            if ((r & titlebarGeometry).isEmpty())
                continue;
            if (titlebarGeometry.top() < r.top()) {
                normalGeo.moveTop(normalGeo.top() + r.top() - titlebarGeometry.top());
                titlebarGeometryAdjusted = true;
            }
        }

        if (!titlebarGeometryAdjusted) {
            normalGeo =
                adjustRectToMakePointVisible(normalGeo, titlebarGeometry.topLeft(), outputRects);
        }
    }

    surface->moveNormalGeometryInOutput(normalGeo.topLeft());
}

OutputListModel *RootSurfaceContainer::outputModel() const
{
    return m_outputModel;
}

void RootSurfaceContainer::moveSurfacesToOutput(const QList<SurfaceWrapper *> &surfaces,
                                                Output *targetOutput,
                                                Output *sourceOutput)
{
    if (!surfaces.isEmpty() && targetOutput) {
        const QRectF targetGeometry = targetOutput->geometry();

        for (auto *surface : surfaces) {
            if (!surface)
                continue;

            const QSizeF size = surface->size();
            QPointF newPos;

            if (surface->ownsOutput() == targetOutput) {
                newPos = surface->position();
            } else {
                const QRectF sourceGeometry =
                    sourceOutput ? sourceOutput->geometry() : surface->ownsOutput()->geometry();
                const QPointF relativePos = surface->position() - sourceGeometry.center();
                newPos = targetGeometry.center() + relativePos;
                surface->setOwnsOutput(targetOutput);
            }
            newPos.setX(
                qBound(targetGeometry.left(), newPos.x(), targetGeometry.right() - size.width()));
            newPos.setY(
                qBound(targetGeometry.top(), newPos.y(), targetGeometry.bottom() - size.height()));
            surface->setPosition(newPos);
        }
    }
}

void RootSurfaceContainer::setupSeatManagement()
{
    auto *helper = Helper::instance();
    if (!helper || !helper->seatManager()) {
        qCWarning(treelandCore) << "Helper or seatManager not available for seat management setup";
        return;
    }

    auto *seatManager = helper->seatManager();
    const auto &seats = seatManager->seats();

    if (seats.isEmpty()) {
        qCWarning(treelandCore) << "No seats available for seat management setup";
        return;
    }

    for (auto *seat : std::as_const(seats)) {
        onSeatAdded(seat);
    }

    // Connect to seat lifecycle signals
    connect(seatManager, &SeatsManager::seatAdded, this, &RootSurfaceContainer::onSeatAdded);
    connect(seatManager, &SeatsManager::seatRemoved, this, &RootSurfaceContainer::onSeatRemoved);
}

void RootSurfaceContainer::setupSurfaceRequestHandlers(SurfaceWrapper *surface)
{
    connect(surface, &SurfaceWrapper::requestMove, this, [this, surface]() {
        WSeat *requestingSeat = determineSeatForRequest(surface);
        if (!requestingSeat) {
            qCWarning(treelandCore) << "No seat available for move request";
            return;
        }

        beginMoveResizeForSeat(requestingSeat, surface, Qt::Edges{});
        auto *helper = Helper::instance();
        if (helper) {
            helper->activateSurface(surface);
        }
    }, Qt::DirectConnection);

    connect(surface, &SurfaceWrapper::requestResize, this, [this, surface](Qt::Edges edges) {
        WSeat *requestingSeat = determineSeatForRequest(surface);
        if (!requestingSeat) {
            qCWarning(treelandCore) << "No seat available for resize request";
            return;
        }

        beginMoveResizeForSeat(requestingSeat, surface, edges);
        if (auto *sh = surface->shellSurface()) {
            sh->setResizeing(true);
        }
        auto *helper = Helper::instance();
        if (helper) {
            helper->activateSurface(surface);
        }
    }, Qt::DirectConnection);

    bool isXdgToplevel = surface->type() == SurfaceWrapper::Type::XdgToplevel;
    bool isXwayland = surface->type() == SurfaceWrapper::Type::XWayland;

    if (isXdgToplevel || isXwayland) {
        connect(surface, &SurfaceWrapper::requestMinimize, this, [this, surface]() {
            auto *helper = Helper::instance();
            if (!helper)
                return;

            if (helper->blockActivateSurface())
                return;

            if (helper->currentMode() == Helper::CurrentMode::Normal) {
                if (surface->surfaceState() == SurfaceWrapper::State::Minimized)
                    return;

                auto container = surface->container();
                if (container) {
                    container->removeSurface(surface);
                }
            }
        });
    }
}

WSeat *RootSurfaceContainer::determineSeatForRequest(SurfaceWrapper *surface)
{
    auto *helper = Helper::instance();
    if (!helper || !helper->seatManager()) {
        return nullptr;
    }

    if (auto *currentEventSeat = helper->currentEventSeat()) {
        return currentEventSeat;
    }

    WSeat *lastInteractingSeat = helper->getLastInteractingSeat(surface);
    if (lastInteractingSeat) {
        return lastInteractingSeat;
    }

    const auto &seats = helper->seatManager()->seats();
    if (!seats.isEmpty()) {
        return seats.first();
    }

    return nullptr;
}

void RootSurfaceContainer::onSeatAdded(WSeat *seat)
{
    if (m_seatContainers.contains(seat))
        return;

    auto *container = new SeatSurfaceManager(seat, this);
    m_seatContainers[seat] = container;

    connect(container, &SeatSurfaceManager::moveResizeChanged, this, &RootSurfaceContainer::moveResizeFinised);
}

void RootSurfaceContainer::onSeatRemoved(WSeat *seat)
{
    auto *container = m_seatContainers.take(seat);
    if (container) {
        qCDebug(treelandCore) << "Removing SeatContainer for seat:" << seat;
        container->deleteLater();
    }
}

SeatSurfaceManager *RootSurfaceContainer::getSeatContainer(WSeat *seat) const
{
    return m_seatContainers.value(seat, nullptr);
}

WSeat *RootSurfaceContainer::getDefaultSeat() const
{
    auto *helper = Helper::instance();
    if (!helper || !helper->seatManager()) {
        qCWarning(treelandCore) << "Helper or seatManager not available";
        return nullptr;
    }

    auto *seatManager = helper->seatManager();

    // Prefer the explicitly designated fallback/primary seat
    if (WSeat *fallback = seatManager->fallbackSeat()) {
        return fallback;
    }

    // Fallback: use first from list (QMap order, not predictable — warn)
    const auto &seats = seatManager->seats();
    if (seats.isEmpty()) {
        qCCritical(treelandCore) << "No seats available - this should not happen in Wayland!";
        return nullptr;
    }
    qCWarning(treelandCore) << "fallbackSeat() is null, using first seat from seats() as default";
    return seats.first();
}

SeatSurfaceManager *RootSurfaceContainer::getSeatContainerOrDefault(WSeat *seat) const
{
    if (!seat) {
        seat = getDefaultSeat();
        if (!seat) {
            qCCritical(treelandCore) << "Cannot get default seat";
            return nullptr;
        }
    }

    auto *container = m_seatContainers.value(seat, nullptr);
    if (!container) {
        qCWarning(treelandCore) << "No container for seat:" << seat;
    }

    return container;
}

void RootSurfaceContainer::beginMoveResizeForSeat(WSeat *seat, SurfaceWrapper *surface, Qt::Edges edges)
{
    if (!surface)
        return;

    auto *container = getSeatContainerOrDefault(seat);
    if (!container)
        return;

    if (container->moveResizeState().surface &&
        container->moveResizeState().surface != surface) {
        container->endMoveResize();
    }

    container->beginMoveResize(surface, edges);

    WSeat *actualSeat = seat ? seat : getDefaultSeat();
    if (actualSeat && actualSeat->cursor()) {
        container->moveResizeState().initialPosition = actualSeat->cursor()->position();
    }
}

void RootSurfaceContainer::doMoveResizeForSeat(WSeat *seat, const QPointF &delta)
{
    auto *container = getSeatContainerOrDefault(seat);
    if (container) {
        container->doMoveResize(delta);
    }
}

void RootSurfaceContainer::endMoveResizeForSeat(WSeat *seat)
{
    auto *container = getSeatContainerOrDefault(seat);
    if (container) {
        container->endMoveResize();
    }
}

SurfaceWrapper *RootSurfaceContainer::getMoveResizeSurfaceForSeat(WSeat *seat) const
{
    auto *container = getSeatContainerOrDefault(seat);
    return container ? container->moveResizeSurface() : nullptr;
}

void RootSurfaceContainer::setActivatedSurfaceForSeat(WSeat *seat, SurfaceWrapper *surface,
                                                      Qt::FocusReason reason)
{
    auto *container = getSeatContainer(seat);
    if (container) {
        container->setActivatedSurface(surface, reason);
    }
}

SurfaceWrapper *RootSurfaceContainer::getActivatedSurfaceForSeat(WSeat *seat) const
{
    auto *container = getSeatContainer(seat);
    if (container) {
        return container->activatedSurface();
    }
    return nullptr;
}
