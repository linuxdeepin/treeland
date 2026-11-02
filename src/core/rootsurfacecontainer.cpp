// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "rootsurfacecontainer.h"
#include "session/session.h"

#include "common/treelandlogging.h"
#include "output/output.h"
#include "core/waylandserver.h"
#include "seat/seatmanager.h"
#include "surface/surfacewrapper.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"
#include "workspace/workspace.h"

#include <wcursor.h>
#include <woutput.h>
#include <wscoplistener.h>
#include <woutputitem.h>
#include <woutputlayout.h>
#include <wxdgpopupsurface.h>
#include <wxdgtoplevelsurface.h>
#include <wseat.h>
#include <winputdevice.h>

#include <QQuickWindow>

WAYLIB_SERVER_USE_NAMESPACE

namespace {

void setPrimaryOutputConfig(Output *output)
{
    auto *server = WaylandServer::instance();
    if (!server)
        return;

    auto *config = server->globalConfig();
    config->setPrimaryOutputId(output->getOutputId());
}

}

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

    // Listen on the native wlr_output_layout change signal (fires on add,
    // remove, reposition and commit-triggered reconfiguration), matching the
    // baseline qw_output_layout::notify_change wiring.
    m_outputLayoutListenerOwner = std::make_unique<WListenerOwner>();
    m_outputLayout->listeners(m_outputLayoutListenerOwner.get())->add(
        &m_outputLayout->handle()->events.change, this, [this] {
        for (auto output : std::as_const(outputs())) {
            output->updatePositionFromLayout();
        }
        ensureCursorVisible();
        // The layout may have moved/removed the output under the cursor, so
        // refresh the cache even without a cursor motion event.
        updateCursorOutput();

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

    QObject::connect(m_cursor, &WCursor::positionChanged, this, [this] {
        m_dragSurfaceItem->setPosition(m_cursor->position());
        updateCursorOutput();
    });

    QObject::connect(m_cursor, &WCursor::requestedDragSurfaceChanged, this, [this] {
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

    wrapper->destroy();
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

    // Layer surfaces are re-entered by LayerSurfaceContainer::addOutput
    // which calls setOutputs. Skip them here to avoid double re-enter.
    for (auto s : std::as_const(surfaces())) {
        if (s->type() == SurfaceWrapper::Type::Layer)
            continue;
        updateSurfaceOutputs(s, SurfaceWrapper::OutputsUpdateReason::AddOutput);
    }

    // A disabled output is dropped by updateSurfaceOutputs(), and a surface can
    // be left with no output at all (e.g. the screen was turned off by DPMS)
    // until it is re-enabled. That is not a geometry change, so nothing would
    // recompute the outputs: the surface would keep an empty frame pacing output
    // and freeze forever. Recompute here once the output is really enabled
    // again, which also covers the first enable commit of a newly added output.
    // A member slot instead of a lambda: Output is recreated (mirror/extension
    // switching) while its WOutput lives on, so the handler must not capture it,
    // and UniqueConnection keeps one handler per WOutput however often the
    // output is registered again.
    QObject::connect(output->output(),
                     &WOutput::enabledChanged,
                     this,
                     &RootSurfaceContainer::onOutputEnabledChanged,
                     Qt::UniqueConnection);
}

void RootSurfaceContainer::onOutputEnabledChanged()
{
    // enabledChanged() carries no argument, read the new state from the sender.
    // Disabling keeps the previous behaviour (the lists are cleared only when
    // something recomputes them); re-enabling is the case that has to heal.
    auto *woutput = qobject_cast<WOutput *>(sender());
    if (!woutput || !woutput->isEnabled())
        return;

    // Layer surfaces keep the outputs their OutputLayerSurfaceContainer gave
    // them: updateSurfaceOutputs() intersects their geometry without filtering
    // disabled outputs, so calling it here would put a layer surface back on an
    // output that is still off. Skip them here for the same reason as the loop in
    // addOutput() above.
    for (auto s : std::as_const(surfaces())) {
        if (s->type() == SurfaceWrapper::Type::Layer)
            continue;
        updateSurfaceOutputs(s, SurfaceWrapper::OutputsUpdateReason::EnabledChanged);
    }
}

void RootSurfaceContainer::removeOutput(Output *output)
{
    m_outputModel->removeObject(output);

    // Move the primary output away from the removed one *before* the sub containers
    // are torn down: LayerSurfaceContainer::removeOutput() re-homes layer surfaces
    // without an explicit output to primaryOutput(), and that must never be the
    // output being removed (its OutputLayerSurfaceContainer is already gone by
    // then).
    movePrimaryOutputAwayFrom(output);

    // Finish any move/resize that grabbed a surface on this output before its
    // containers are torn down, so the seats still see them (same order as
    // removeBySubContainer()).
    for (auto *container : std::as_const(m_seatContainers)) {
        if (container->moveResizeSurface() &&
            container->moveResizeSurface()->ownsOutput() == output) {
            container->endMoveResize();
        }
    }

    m_outputLayout->remove(output->output());
    SurfaceContainer::removeOutput(output);

    // ensure cursor within output
    const auto outputPos = output->outputItem()->position();
    if (output->geometry().contains(m_cursor->position()) && m_primaryOutput) {
        const auto posInOutput = m_cursor->position() - outputPos;
        const auto newCursorPos = m_primaryOutput->outputItem()->position() + posInOutput;

        if (m_primaryOutput->geometry().contains(newCursorPos))
            WaylandServer::instance()->setCursorPosition(newCursorPos);
        else
            WaylandServer::instance()->setCursorPosition(m_primaryOutput->geometry().center());
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

bool RootSurfaceContainer::isInMoveResize() const
{
    return isInMoveResizeForSeat(nullptr);
}

void RootSurfaceContainer::startMove(SurfaceWrapper *surface)
{
    if (beginMoveResizeForSeat(nullptr, surface, Qt::Edges{}))
        WaylandServer::instance()->activateSurface(surface);
}

void RootSurfaceContainer::startResize(SurfaceWrapper *surface, Qt::Edges edges)
{
    Q_ASSERT(edges != 0);
    if (!surface || !surface->isResizable())
        return;

    if (!beginMoveResizeForSeat(nullptr, surface, edges))
        return;

    surface->shellSurface()->setResizeing(true);
    WaylandServer::instance()->activateSurface(surface);
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
            auto output = parentSurface ? parentSurface->ownsOutput() : cursorOutput();
            if (!output)
                output = primaryOutput();

            if (auto xdgPopupSurface = qobject_cast<WXdgPopupSurface *>(surface->shellSurface())) {
                if (parentSurface->type() != SurfaceWrapper::Type::Layer) {
                    // If parentSurface is Layer surface, follow parentSurface->ownsOutput
                    auto pos = parentSurface->position() + parentSurface->surfaceItem()->position()
                        + xdgPopupSurface->getPopupPosition();
                    if (auto op = wlr_output_layout_output_at(m_outputLayout->handle(), pos.x(), pos.y()))
                        output =
                            WaylandServer::instance()->getOutput(WOutput::fromHandle(op));
                }
            }
            surface->setOwnsOutput(output);
        }

        connect(surface, &SurfaceWrapper::geometryChanged, this, [this, surface] {
            updateSurfaceOutputs(surface, SurfaceWrapper::OutputsUpdateReason::GeometryChanged);
        });

        updateSurfaceOutputs(surface, SurfaceWrapper::OutputsUpdateReason::SurfaceAdded);
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
                                                        [[maybe_unused]] const QRectF &oldGeometry)
{
    // Keep the opposite edge at its position when the drag began.
    auto adjustForResize = [](QRectF &geometry,
                              const QRectF &startGeometry,
                              Qt::Edges edges) {
        if (edges & Qt::RightEdge)
            geometry.moveLeft(startGeometry.left());
        if (edges & Qt::BottomEdge)
            geometry.moveTop(startGeometry.top());
        if (edges & Qt::LeftEdge)
            geometry.moveRight(startGeometry.right());
        if (edges & Qt::TopEdge)
            geometry.moveBottom(startGeometry.bottom());
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
                QRectF adjustedGeo = newGeometry;
                adjustForResize(adjustedGeo, mrState.startGeometry, edges);

                if (adjustedGeo.topLeft() != newGeometry.topLeft()) {
                    newGeometry = adjustedGeo;
                    mrState.settingPositionFlag = true;
                    surface->setPosition(newGeometry.topLeft());
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
    return m_cursorOutput;
}

void RootSurfaceContainer::updateCursorOutput()
{
    Output *output = outputAt(m_cursor->position());
    if (output == m_cursorOutput)
        return;

    m_cursorOutput = output;
    Q_EMIT cursorOutputChanged(output);
}

Output *RootSurfaceContainer::outputAt(const QPointF &pos) const
{
    auto o = wlr_output_layout_output_at(m_outputLayout->handle(), pos.x(), pos.y());
    if (!o)
        return nullptr;

    return WaylandServer::instance()->getOutput(WOutput::fromHandle(o));
}

Output *RootSurfaceContainer::primaryOutput() const
{
    return m_primaryOutput;
}

void RootSurfaceContainer::setPrimaryOutput(Output *newPrimaryOutput, bool updateDconfig)
{
    if (m_primaryOutput == newPrimaryOutput)
        return;

    m_primaryOutput = newPrimaryOutput;
    Q_EMIT primaryOutputChanged();
    if (updateDconfig)
        setPrimaryOutputConfig(newPrimaryOutput);
}

void RootSurfaceContainer::movePrimaryOutputAwayFrom(const Output *output)
{
    if (!output || m_primaryOutput != output)
        return;

    // Entries of the layout can be in intermediate states (copy mode replacement,
    // pending outputs) where getOutput() cannot resolve them, so skip those instead
    // of giving up on the first one. Without a remaining output the primary becomes
    // null: layer surfaces created without an explicit output are then closed
    // instead of being re-homed to the output being removed.
    for (auto *o : m_outputLayout->outputs()) {
        if (o == output->output()) {
            continue;
        }
        if (auto *candidate = WaylandServer::instance()->getOutput(o)) {
            setPrimaryOutput(candidate);
            return;
        }
    }

    setPrimaryOutput(nullptr);
}

const QList<Output *> &RootSurfaceContainer::outputs() const
{
    return m_outputModel->objects();
}

void RootSurfaceContainer::ensureCursorVisible()
{
    const auto cursorPos = m_cursor->position();
    if (wlr_output_layout_output_at(m_outputLayout->handle(), cursorPos.x(), cursorPos.y()))
        return;

    if (m_primaryOutput) {
        WaylandServer::instance()->setCursorPosition(m_primaryOutput->geometry().center());
    }
}

void RootSurfaceContainer::updateSurfaceOutputs(SurfaceWrapper *surface,
                                                SurfaceWrapper::OutputsUpdateReason reason)
{
    QList<WOutput *> outputs;

    const auto &allOutputs = m_outputModel->objects();
    int enabledOutputs = 0;
    for (auto *output : allOutputs) {
        if (output->output()->isEnabled())
            ++enabledOutputs;
    }

    if (surface->type() != SurfaceWrapper::Type::Layer) {
        for (auto *output : m_outputModel->objects()) {
            if (!output->output()->isEnabled())
                continue;

            outputs.append(output->output());
            if (outputs.size() > 1)
                break;
        }

        if (outputs.size() != 1) {
            const QRectF geometry = surface->geometry();
            outputs = m_outputLayout->getIntersectedOutputs(geometry.toRect());
            outputs.removeIf([](WOutput *output) {
                return !output->isEnabled();
            });
        }
    } else {
        // Unreachable today: every caller skips layer surfaces (see the loops in
        // addOutput() and onOutputEnabledChanged()). This branch has no
        // isEnabled() filter, so making it reachable needs one first, or a layer
        // surface lands back on an output that is still off.
        const QRectF geometry = surface->geometry();
        outputs = m_outputLayout->getIntersectedOutputs(geometry.toRect());
    }
    // A new window's initial position is (0,0) before placement, so geometry-based
    // intersection may not include ownsOutput. Add it to keep outputs consistent.
    if (surface->positionAutomatic() && surface->ownsOutput()
        && !outputs.contains(surface->ownsOutput()->output()))
        outputs.append(surface->ownsOutput()->output());
    surface->noteOutputsUpdate(reason,
                               enabledOutputs,
                               static_cast<int>(allOutputs.size()),
                               static_cast<int>(outputs.size()));
    surface->setOutputs(outputs, /*recordDiagnostics=*/true);

    if (auto *ws = WaylandServer::instance()->workspace())
        ws->updateSurfaceOwnsOutput(surface);
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
    qreal minScreenX = std::numeric_limits<qreal>::max();
    outputRects.reserve(outputs().size());
    for (auto o : std::as_const(outputs())) {
        outputRects << o->validGeometry();
        minScreenX = qMin(minScreenX, o->validGeometry().left());
    }

    // Ensure window is not outside the screen
    QPointF mustVisiblePosOfSurface(qMin(normalGeo.right(), normalGeo.x() + 20),
                                    qMin(normalGeo.bottom(), normalGeo.y() + 20));

    if (normalGeo.x() < minScreenX) {
        mustVisiblePosOfSurface.setX(qMax(normalGeo.left(), normalGeo.right() - 20));
    }

    normalGeo = adjustRectToMakePointVisible(normalGeo, mustVisiblePosOfSurface, outputRects);

    // Ensure titlebar is not above the screen
    const auto titlebarGeometry = surface->titlebarGeometry().translated(normalGeo.topLeft());
    if (titlebarGeometry.isValid()) {
        for (auto r : std::as_const(outputRects)) {
            if ((r & titlebarGeometry).isEmpty())
                continue;
            if (titlebarGeometry.top() < r.top()) {
                normalGeo.moveTop(normalGeo.top() + r.top() - titlebarGeometry.top());
                break; // Only need to adjust once based on the intersected output
            }
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
            if (newPos.x() + size.width() > targetGeometry.right())
                newPos.setX(targetGeometry.right() - size.width());
            if (newPos.x() < targetGeometry.left())
                newPos.setX(targetGeometry.left());

            if (newPos.y() + size.height() > targetGeometry.bottom())
                newPos.setY(targetGeometry.bottom() - size.height());
            if (newPos.y() < targetGeometry.top())
                newPos.setY(targetGeometry.top());

            surface->setPosition(newPos);
        }
    }
}

void RootSurfaceContainer::setupSeatManagement()
{
    auto *server = WaylandServer::instance();
    if (!server || !server->seatManager()) {
        qCWarning(lcTlCore) << "WaylandServer or seatManager not available for seat management setup";
        return;
    }

    auto *seatManager = server->seatManager();
    const auto &seats = seatManager->seats();

    if (seats.isEmpty()) {
        qCWarning(lcTlCore) << "No seats available for seat management setup";
        return;
    }

    for (auto *seat : std::as_const(seats)) {
        onSeatAdded(seat);
    }

    // Connect to seat lifecycle signals
    connect(seatManager, &SeatManager::seatAdded, this, &RootSurfaceContainer::onSeatAdded);
    connect(seatManager, &SeatManager::seatRemoved, this, &RootSurfaceContainer::onSeatRemoved);
}

void RootSurfaceContainer::setupSurfaceRequestHandlers(SurfaceWrapper *surface)
{
    connect(
        surface,
        &SurfaceWrapper::moveRequested,
        this,
        [this, surface]() {
            WSeat *requestingSeat = determineSeatForRequest(surface);
            if (!requestingSeat) {
                qCWarning(lcTlCore) << "No seat available for move request";
                return;
            }

            if (!beginMoveResizeForSeat(requestingSeat, surface, Qt::Edges{}))
                return;
            auto *server = WaylandServer::instance();
            if (server) {
                server->activateSurface(surface);
            }
        },
        Qt::DirectConnection);

    connect(
        surface,
        &SurfaceWrapper::resizeRequested,
        this,
        [this, surface](Qt::Edges edges) {
            WSeat *requestingSeat = determineSeatForRequest(surface);
            if (!requestingSeat) {
                qCWarning(lcTlCore) << "No seat available for resize request";
                return;
            }

            if (!beginMoveResizeForSeat(requestingSeat, surface, edges))
                return;
            if (auto *sh = surface->shellSurface()) {
                sh->setResizeing(true);
            }
            auto *server = WaylandServer::instance();
            if (server) {
                server->activateSurface(surface);
            }
        },
        Qt::DirectConnection);
}

WSeat *RootSurfaceContainer::determineSeatForRequest(SurfaceWrapper *surface)
{
    auto *server = WaylandServer::instance();
    if (!server || !server->seatManager()) {
        return nullptr;
    }

    if (auto *currentEventSeat = server->currentEventSeat()) {
        return currentEventSeat;
    }

    WSeat *lastInteractingSeat = server->getLastInteractingSeat(surface);
    if (lastInteractingSeat) {
        return lastInteractingSeat;
    }

    const auto &seats = server->seatManager()->seats();
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
    connect(container, &SeatSurfaceManager::activatedSurfaceChanged, this, [seat]() {
        auto *server = WaylandServer::instance();
        if (server && server->seat() == seat) {
            Q_EMIT server->activatedSurfaceChanged();
        }
    });
}

void RootSurfaceContainer::onSeatRemoved(WSeat *seat)
{
    auto *container = m_seatContainers.take(seat);
    if (container) {
        qCDebug(lcTlCore) << "Removing SeatContainer for seat:" << seat;
        delete container;
    }
}

SeatSurfaceManager *RootSurfaceContainer::getSeatContainer(WSeat *seat) const
{
    return m_seatContainers.value(seat, nullptr);
}

WSeat *RootSurfaceContainer::getDefaultSeat() const
{
    auto *server = WaylandServer::instance();
    if (!server || !server->seatManager()) {
        qCWarning(lcTlCore) << "WaylandServer or seatManager not available";
        return nullptr;
    }

    auto *seatManager = server->seatManager();

    // Prefer the explicitly designated fallback/primary seat
    if (WSeat *fallback = seatManager->fallbackSeat()) {
        return fallback;
    }

    // Fallback: use first from list (QMap order, not predictable — warn)
    const auto &seats = seatManager->seats();
    if (seats.isEmpty()) {
        qCCritical(lcTlCore) << "No seats available - this should not happen in Wayland!";
        return nullptr;
    }
    qCWarning(lcTlCore) << "fallbackSeat() is null, using first seat from seats() as default";
    return seats.first();
}

SeatSurfaceManager *RootSurfaceContainer::getSeatContainerOrDefault(WSeat *seat) const
{
    if (!seat) {
        seat = getDefaultSeat();
        if (!seat) {
            qCCritical(lcTlCore) << "Cannot get default seat";
            return nullptr;
        }
    }

    auto *container = m_seatContainers.value(seat, nullptr);
    if (!container) {
        qCWarning(lcTlCore) << "No container for seat:" << seat;
    }

    return container;
}

void RootSurfaceContainer::dismissAllPopups()
{
    for (auto *container : std::as_const(m_seatContainers)) {
        container->dismissPopups();
    }
}

void RootSurfaceContainer::givePopupFocus(SurfaceWrapper *popupWrapper)
{
    if (!popupWrapper)
        return;

    // Route to the correct seat based on wlr_xdg_popup->seat.
    auto *popupSurface = qobject_cast<WXdgPopupSurface *>(popupWrapper->shellSurface());
    if (!popupSurface)
        return;

    auto *wlrPopup = popupSurface->handle();
    if (!wlrPopup || !wlrPopup->seat)
        return;

    for (auto it = m_seatContainers.constBegin(); it != m_seatContainers.constEnd(); ++it) {
        if (it.key()->handle() == wlrPopup->seat) {
            it.value()->givePopupFocus(popupWrapper);
            return;
        }
    }
}

bool RootSurfaceContainer::beginMoveResizeForSeat(WSeat *seat, SurfaceWrapper *surface, Qt::Edges edges)
{
    if (!surface)
        return false;

    auto *container = getSeatContainerOrDefault(seat);
    if (!container)
        return false;

    // Geometry changes can only use one seat's drag state for a given surface.
    for (auto *other : std::as_const(m_seatContainers)) {
        if (other != container && other->moveResizeSurface() == surface)
            return false;
    }

    if (container->moveResizeState().surface &&
        container->moveResizeState().surface != surface) {
        container->endMoveResize();
    }

    const auto state = surface->surfaceState();
    const bool wasTiled = edges == Qt::Edges()
        && (state == SurfaceWrapper::State::Tiling || state == SurfaceWrapper::State::Maximized);
    const QRectF tiledGeo = wasTiled ? surface->geometry() : QRectF();

    container->beginMoveResize(surface, edges);
    if (container->moveResizeSurface() != surface)
        return false;

    WSeat *actualSeat = seat ? seat : getDefaultSeat();
    const bool hasCursor = actualSeat && actualSeat->cursor();
    const QPointF cursor = hasCursor ? actualSeat->cursor()->position() : QPointF();

    // De-tile: restore saved size, reposition so the cursor stays at the same
    // relative grab ratio within the window
    if (wasTiled && !tiledGeo.isEmpty() && hasCursor) {
        const QSizeF restoredSize = surface->normalGeometry().isValid()
            ? surface->normalGeometry().size()
            : tiledGeo.size();
        const qreal fx =
            tiledGeo.width() > 0 ? (cursor.x() - tiledGeo.left()) / tiledGeo.width() : 0;
        const qreal fy =
            tiledGeo.height() > 0 ? (cursor.y() - tiledGeo.top()) / tiledGeo.height() : 0;
        QPointF newPos(cursor.x() - fx * restoredSize.width(),
                       cursor.y() - fy * restoredSize.height());
        newPos = surface->alignToPixelGrid(newPos);
        surface->setPosition(newPos);
        container->moveResizeState().startGeometry = QRectF(newPos, restoredSize);
    }

    if (actualSeat && actualSeat->cursor())
        container->moveResizeState().initialPosition = cursor;

    return true;
}

void RootSurfaceContainer::doMoveResizeForSeat(WSeat *seat, const QPointF &delta)
{
    auto *container = getSeatContainerOrDefault(seat);
    if (container) {
        container->doMoveResize(delta);
    }
}

QQuickItem *RootSurfaceContainer::ensureEdgeTilePreview()
{
    if (m_edgeTilePreview)
        return m_edgeTilePreview;

    auto *engine = WaylandServer::instance()->qmlEngine();
    if (!engine)
        return nullptr;

    m_edgeTilePreview = engine->createEdgeTilePreview(this);
    if (m_edgeTilePreview)
        m_edgeTilePreview->setZ(OverlayZOrder);
    return m_edgeTilePreview;
}

void RootSurfaceContainer::updateEdgeTilePreview(SurfaceWrapper::TileMode mode, Output *out, WSeat *seat)
{
    auto *preview = ensureEdgeTilePreview();
    if (!preview)
        return;

    if (mode == SurfaceWrapper::TileMode::None || !out) {
        preview->setVisible(false);
        return;
    }

    const QRectF geo = out->tileGeometry(mode);
    if (!geo.isValid()) {
        preview->setVisible(false);
        return;
    }

    // Use the dragged surface's current geometry as the animation starting
    // point, so the preview grows from the window (mirrors KWin's outline).
    QRectF sourceGeo;
    if (auto *container = getSeatContainerOrDefault(seat)) {
        if (SurfaceWrapper *surface = container->moveResizeSurface())
            sourceGeo = surface->geometry();
    }

    preview->setProperty("sourceGeometry", QVariant::fromValue(sourceGeo));
    preview->setProperty("targetGeometry", QVariant::fromValue(geo));
    preview->setVisible(true);
}

void RootSurfaceContainer::detectEdgeTilingForSeat(WSeat *seat)
{
    auto *container = getSeatContainerOrDefault(seat);
    if (!container)
        return;

    auto *cfg = WaylandServer::instance()->config();
    const qreal sideTrigger = cfg ? qreal(cfg->edgeSideTriggerDistance()) : 20.0;
    const qreal topTrigger = cfg ? qreal(cfg->edgeTopTriggerDistance()) : 5.0;
    const qreal quadRatio = cfg ? qBound<qreal>(0.0, qreal(cfg->edgeQuadrantZoneRatio()), 0.5) : 0.25;
    auto &mrState = container->moveResizeState();
    SurfaceWrapper::TileMode mode = SurfaceWrapper::TileMode::None;
    Output *out = nullptr;
    bool innerBorder = false;

    if (mrState.surface && mrState.edges == Qt::Edges()) {
        WSeat *actualSeat = seat ? seat : getDefaultSeat();
        WCursor *cursor = actualSeat ? actualSeat->cursor() : nullptr;
        const QPointF pos = cursor ? cursor->position() : QPointF();
        out = outputAt(pos);
        if (out) {
            const QRectF area = out->validGeometry();
            const qreal quadTop = area.top() + area.height() * quadRatio;
            const qreal quadBottom = area.bottom() - area.height() * quadRatio;
            if (pos.x() <= area.left() + sideTrigger) {
                // Left edge: the top/bottom quadrant zones tile to the corner.
                if (pos.y() <= quadTop)
                    mode = SurfaceWrapper::TileMode::TopLeft;
                else if (pos.y() >= quadBottom)
                    mode = SurfaceWrapper::TileMode::BottomLeft;
                else
                    mode = SurfaceWrapper::TileMode::Left;
            } else if (pos.x() >= area.right() - sideTrigger) {
                // Right edge: the top/bottom quadrant zones tile to the corner.
                if (pos.y() <= quadTop)
                    mode = SurfaceWrapper::TileMode::TopRight;
                else if (pos.y() >= quadBottom)
                    mode = SurfaceWrapper::TileMode::BottomRight;
                else
                    mode = SurfaceWrapper::TileMode::Right;
            } else if (pos.y() <= area.top() + topTrigger) {
                // Top edge: maximize only; quadrant zones are exclusive to
                // the left/right edges.
                mode = SurfaceWrapper::TileMode::Maximize;
            }

            if (mode != SurfaceWrapper::TileMode::None
                && !mrState.surface->supportsTileMode(mode)) {
                mode = SurfaceWrapper::TileMode::None;
            }

            // Multi-screen inner-edge detection:
            // Sample 1px outside the edge; if another output covers that
            // point, this is an inner edge
            if (mode != SurfaceWrapper::TileMode::None) {
                QPointF samplePt;
                switch (mode) {
                case SurfaceWrapper::TileMode::Maximize:
                    samplePt = QPointF(pos.x(), area.top() - 1.0);
                    break;
                case SurfaceWrapper::TileMode::Left:
                case SurfaceWrapper::TileMode::TopLeft:
                case SurfaceWrapper::TileMode::BottomLeft:
                    samplePt = QPointF(area.left() - 1.0, pos.y());
                    break;
                case SurfaceWrapper::TileMode::Right:
                case SurfaceWrapper::TileMode::TopRight:
                case SurfaceWrapper::TileMode::BottomRight:
                    samplePt = QPointF(area.right(), pos.y());
                    break;
                case SurfaceWrapper::TileMode::None:
                    break;
                }
                for (Output *o : outputs()) {
                    if (o == out)
                        continue;
                    if (o->geometry().contains(samplePt)) {
                        innerBorder = true;
                        break;
                    }
                }
            }
        }
    }

    // Re-evaluate when the edge state changes, or when the cursor crosses to
    // a different output while the preview is active
    if (mrState.detectedTileMode != mode || mrState.edgeTileInnerBorder != innerBorder
        || out != mrState.detectedTileOutput) {
        mrState.detectedTileMode = mode;
        mrState.edgeTileInnerBorder = innerBorder;
        mrState.detectedTileOutput = out;

        if (mode == SurfaceWrapper::TileMode::None) {
            container->stopEdgeTileDelay();
            mrState.edgeTilePreviewActive = false;
            updateEdgeTilePreview(SurfaceWrapper::TileMode::None, nullptr);
        } else if (innerBorder) {
            container->stopEdgeTileDelay();
            mrState.edgeTilePreviewActive = false;
            updateEdgeTilePreview(SurfaceWrapper::TileMode::None, nullptr);
            container->startEdgeTileDelay();
        } else {
            container->stopEdgeTileDelay();
            mrState.edgeTilePreviewActive = true;
            updateEdgeTilePreview(mode, out, seat);
        }
    }
}

void RootSurfaceContainer::endMoveResizeForSeat(WSeat *seat)
{
    auto *container = getSeatContainerOrDefault(seat);
    if (container) {
        container->endMoveResize(/*alignLayerPosition=*/true);
    }
    updateEdgeTilePreview(SurfaceWrapper::TileMode::None, nullptr);
}

void RootSurfaceContainer::cancelMoveResizeForSeat(WSeat *seat)
{
    auto *container = getSeatContainerOrDefault(seat);
    if (container) {
        container->cancelMoveResize();
    }
    updateEdgeTilePreview(SurfaceWrapper::TileMode::None, nullptr);
}

SurfaceWrapper *RootSurfaceContainer::getMoveResizeSurfaceForSeat(WSeat *seat) const
{
    auto *container = getSeatContainerOrDefault(seat);
    return container ? container->moveResizeSurface() : nullptr;
}

bool RootSurfaceContainer::isInMoveResizeForSeat(WSeat *seat) const
{
    auto *container = getSeatContainer(seat);
    return container && container->moveResizeState().surface;
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
