// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "focuscontroller.h"
#include "waylandserver.h"
#include "rootsurfacecontainer.h"
#include "seat/seatmanager.h"
#include "surface/surfacewrapper.h"
#include "workspace/workspace.h"
#include "common/treelandlogging.h"

#include <wserver.h>
#include <wxwayland.h>
#include <wsurfaceitem.h>
#include <wxdgpopupsurface.h>
#include <wxwaylandsurface.h>
#include <woutputrenderwindow.h>

#include <wlr_all.h>

#include <QDateTime>

FocusController::FocusController(WaylandServer *server)
    : QObject(server)
    , m_server(server)
{}

void FocusController::activateSurface(SurfaceWrapper *wrapper,
                             Qt::FocusReason reason,
                             WSeat *seat,
                             bool raise)
{
    if (wrapper && wrapper->isIMCandidatePanel())
        return;

    if (wrapper && !wrapper->acceptKeyboardFocus()) {
        if (raise) {
            wrapper->stackToLast();
        }

        return;
    }

    // Plain activation: if the deepest modal is minimized, refuse to activate the parent
    // entirely. The user must explicitly unminimize the modal first (e.g., click it).
    SurfaceWrapper *originalWrapper = wrapper;
    if (wrapper) {
        if (SurfaceWrapper *modal = wrapper->findModal()) {
            if (modal != wrapper) {
                if (modal->workspaceId() != wrapper->workspaceId()
                    && wrapper->workspaceId() != -1) {
                    m_server->workspace()->moveSurfaceTo(modal, wrapper->workspaceId());
                }

                if (modal->isMinimized()) {
                    qCCritical(lcTlShell) << "Refusing to activate parent with minimized modal"
                                          << "parent =" << wrapper << "modal =" << modal;
                    return;
                }

                originalWrapper->stackToLast();
                wrapper = modal;
            }
        }
    }

    if (m_server->blockActivateSurface() && wrapper && wrapper->type() != SurfaceWrapper::Type::LockScreen) {
        if (wrapper->hasActiveCapability()) {
            m_server->workspace()->pushActivedSurface(wrapper);
        }
        return;
    }

    if (!wrapper || wrapper->hasActiveCapability()) {
        setActivatedSurface(wrapper, seat, raise);
    } else {
        qCCritical(lcTlShell)
            << "Trying to activate a surface which doesn't have ActiveCapability!";
    }

    if (!wrapper || wrapper->hasFocusCapability()) {
        requestKeyboardFocus(wrapper, reason, seat);
    }
}

void FocusController::forceActivateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason, WSeat *seat)
{
    if (!wrapper) {
        qCCritical(lcTlShell) << "Don't force activate to empty surface! do you want `WaylandServer::activeSurface(nullptr)`?";
        return;
    }
    if (!wrapper->shellSurface()) {
        qCWarning(lcTlShell) << "Try to force activate a destroyed surface!";
        return;
    }

    // Forced activation: the modal's minimized state is irrelevant — the caller explicitly
    // requested this surface. However, if the parent itself is minimized, it must be restored
    // here because the modal (which may already be unminimized) won't trigger parent linkage.
    SurfaceWrapper *originalWrapper = wrapper;
    const bool restoreAnimation =
        !(reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason);
    if (SurfaceWrapper *modal = wrapper->findModal()) {
        if (modal != wrapper) {
            if (modal->workspaceId() != wrapper->workspaceId() && wrapper->workspaceId() != -1) {
                m_server->workspace()->moveSurfaceTo(modal, wrapper->workspaceId());
            }
            if (originalWrapper->isMinimized()) {
                originalWrapper->restoreFromMinimized(restoreAnimation);
            }
            originalWrapper->stackToLast();
            wrapper = modal;
        }
    }

    m_server->restoreFromShowDesktop(wrapper);

    if (wrapper->isMinimized()) {
        wrapper->restoreFromMinimized(restoreAnimation);
    }

    if (!wrapper->surface()->mapped()) {
        qCWarning(lcTlShell) << "Can't activate unmapped surface: " << wrapper;
        return;
    }

    if (!wrapper->showOnWorkspace(m_server->workspace()->current()->id()))
        m_server->workspace()->switchTo(m_server->workspace()->modelIndexOfSurface(wrapper));
    activateSurface(wrapper, reason, seat);
}

SurfaceWrapper *FocusController::keyboardFocusSurface() const
{
    auto item = m_server->window()->activeFocusItem();
    if (!item) {
        return nullptr;
    }

    auto surface = qobject_cast<WSurfaceItem *>(item->parent());
    if (!surface) {
        return nullptr;
    }

    return qobject_cast<SurfaceWrapper *>(surface->parent());
}

SurfaceWrapper *FocusController::activatedSurface() const
{
    if (!m_server->rootContainer())
        return nullptr;

    auto *seatContainer = m_server->rootContainer()->getSeatContainerOrDefault(m_server->seat());
    return seatContainer ? seatContainer->activatedSurface() : nullptr;
}

void FocusController::setActivatedSurface(SurfaceWrapper *newActivateSurface, WSeat *seat, bool raise)
{
    if (!m_server->rootContainer()) {
        qCWarning(lcTlCore) << "Cannot set activated surface: root surface container is null";
        return;
    }

    WSeat *targetSeat = seat ? seat : m_server->seat();
    auto *seatContainer = m_server->rootContainer()->getSeatContainerOrDefault(targetSeat);
    if (!seatContainer) {
        qCWarning(lcTlCore) << "Cannot set activated surface: no seat container for seat"
                            << targetSeat;
        return;
    }

    if (seatContainer->activatedSurface() == newActivateSurface) {
        return;
    }

    const bool isPrimarySeat = (targetSeat == m_server->seat());
    auto *oldPrimarySurface = isPrimarySeat ? activatedSurface() : nullptr;

    if (oldPrimarySurface)
        oldPrimarySurface->setActivate(false);

    bool wasShowingDesktop = false;

    if (newActivateSurface) {
        Q_ASSERT(newActivateSurface->showOnWorkspace(m_server->workspace()->current()->id()));
        if (raise) {
            newActivateSurface->stackToLast();
            if (newActivateSurface->type() == SurfaceWrapper::Type::XWayland) {
                auto xwaylandSurface =
                    qobject_cast<WXWaylandSurface *>(newActivateSurface->shellSurface());
                // wlroots only links a mapped window into its stack list (an unmapped window is
                // pushed below by its own map handler), and restack() must not be called for
                // override-redirect windows.
                if (xwaylandSurface && xwaylandSurface->isX11Mapped()
                    && !xwaylandSurface->isBypassManager()) {
                    xwaylandSurface->restack(nullptr, WXWaylandSurface::XCB_STACK_MODE_ABOVE);
                }
            }
        }
    }

    if (newActivateSurface) {
        if (m_server->showDesktopState() == ShowDesktopInterfaceV1::State::Show) {
            m_server->cancelShowDesktop(newActivateSurface);
            newActivateSurface->setHideByShowDesk(true);
            wasShowingDesktop = true;
        }

        Q_ASSERT(newActivateSurface->hasActiveCapability());
        m_server->workspace()->pushActivedSurface(newActivateSurface);
    }

    // SeatSurfaceManager emits activatedSurfaceChanged, and RootSurfaceContainer forwards
    // it for the primary seat. Do not emit WaylandServer::activatedSurfaceChanged again here.
    seatContainer->setActivatedSurface(newActivateSurface, Qt::OtherFocusReason);

    // This also restores keyboard focus on the other seats; the caller's subsequent
    // requestKeyboardFocus() only covers the target seat.
    if (wasShowingDesktop)
        m_server->restoreShowDesktopFocus();

    if (isPrimarySeat && newActivateSurface) {
        Q_ASSERT(newActivateSurface->hasActiveCapability());
        newActivateSurface->setActivate(true);
    }

}

void FocusController::onRenderWindowActiveFocusItemChanged()
{
    if (!keyboardFocusSurface()) {
        // Keyboard focus moved to a non-client window (e.g. internal QML component).
        // Notify all seats to clear the keyboard focus surface.
        const auto seats = m_server->seatManager()->seats();
        for (auto *seat : seats) {
            if (auto *seatContainer = m_server->rootContainer()->getSeatContainer(seat)) {
                if (seatContainer->keyboardFocusSurface())
                    seatContainer->setKeyboardFocusSurface(nullptr);
            }
        }
    }
}

void FocusController::requestKeyboardFocus(SurfaceWrapper *wrapper, Qt::FocusReason reason, WSeat *seat)
{
    if (wrapper) {
        // Pop up through parent hierarchy until we find a non-popup surface or grabbed popup
        while (wrapper) {
            auto *popupSurface = qobject_cast<WXdgPopupSurface *>(wrapper->shellSurface());
            if (!popupSurface)
                break;
            if (popupSurface->handle()->seat)
                break;
            wrapper = wrapper->parentSurface();
        }
        if (!wrapper || !wrapper->hasFocusCapability()) {
            qCDebug(lcTlShell) << "Request keyboard focus for surface without focus capability!"
                                 << "surface =" << wrapper;
            return;
        }
    }

    if (!seat)
        seat = m_server->seat();

    // Delegate to SeatSurfaceManager which handles keyboardFocusPriority checks,
    // Qt focus management, Wayland focus, multi-seat arbitration, and interaction metadata.
    auto *seatContainer = m_server->rootContainer()->getSeatContainer(seat);
    Q_ASSERT(seatContainer);
    seatContainer->setKeyboardFocusSurface(wrapper, reason);
}

WSeat *FocusController::getLastInteractingSeat(SurfaceWrapper *surface) const
{
    if (!surface) {
        return nullptr;
    }

    auto lastSeatVariant = surface->property("lastInteractingSeat");
    if (lastSeatVariant.isValid()) {
        auto seat = lastSeatVariant.value<WSeat*>();
        if (m_server->seatManager()->seats().contains(seat)) {
            return seat;
        }
    }
    return nullptr;
}

void FocusController::updateSurfaceSeatInteraction(SurfaceWrapper *surface, WSeat *seat)
{
    if (!surface || !seat)
        return;

    surface->setProperty("lastInteractingSeat", QVariant::fromValue(seat));
    surface->setProperty("lastInteractionTime", QDateTime::currentMSecsSinceEpoch());
}

void FocusController::installActivationProtocol(WServer *server)
{
    // The window transition manager is part of the activation path: an
    // activation token may carry a pending transition rect that has to be
    // associated with the target surface before it maps.
    m_windowTransitionManagerV1 = server->attach<WindowTransitionManagerInterfaceV1>();
    auto *activation = server->attach<ActivationManagerInterfaceV1>(
        [this](WSurface *surface, WSeat *seat) { return canTransferActivation(surface, seat); });
    connect(activation, &ActivationManagerInterfaceV1::activateRequested,
            this, &FocusController::onActivationRequested);
}
bool FocusController::canTransferActivation(WSurface *surface, WSeat *seat) const
{
    // Determine whether the surface can transfer activation for the same seat
    // that produced the serial in set_serial.
    if (!seat || !seat->isValid()) {
        return false;
    }

    auto wrapper = m_server->rootContainer()->getSurface(surface);
    if (!wrapper) {
        return false;
    }

    if (seat->keyboardFocusSurface() == surface) {
        return true;
    }

    if (seat->pointerFocusSurface() == surface) {
        return true;
    }

    if (wrapper->isActivated() && getLastInteractingSeat(wrapper) == seat) {
        return true;
    }

    return false;
}
void FocusController::onActivationRequested(const QString &token,
                                           ActivationManagerInterfaceV1::TokenDisposition disposition,
                                           WSurface *wsurface, WSeat *seat,
                                           WSurface *originatingSurface)
{
    auto wrapper = m_server->rootContainer()->getSurface(wsurface);
    if (!wrapper) {
        qCWarning(lcTlCore) << "Activation request for unknown surface!";
        return;
    }

    if (m_windowTransitionManagerV1
        && m_windowTransitionManagerV1->hasPendingWindowTransitionRect(token)) {
        if (!originatingSurface) {
            // Without an originating surface the rect cannot be mapped to
            // global coordinates, so it will never be used. Close it now
            // instead of waiting for the pending sweep.
            qCDebug(lcTlWindowTransition) << "Window transition rect discarded: token has no"
                                          << "originating surface"
                                          << "appId:" << wrapper->appId();
            m_windowTransitionManagerV1->discardPendingRect(token);
        } else if (wsurface->mapped()) {
            qCDebug(lcTlWindowTransition) << "Window transition rect discarded: target surface"
                                          << "is already mapped (client activation arrived too late)"
                                          << "appId:" << wrapper->appId();
            // Discard the pending rect object if any.
            m_windowTransitionManagerV1->discardPendingRect(token);
        } else {
            auto *originWrapper = m_server->rootContainer()->getSurface(originatingSurface);
            if (originWrapper) {
                bool associated = m_windowTransitionManagerV1->associatePendingRect(
                    token, wrapper->windowTransition(), originWrapper->windowTransition());
                if (!associated) {
                    qCDebug(lcTlWindowTransition)
                        << "Window transition rect was destroyed"
                        << "before activation, using default animation"
                        << "appId:" << wrapper->appId();
                }
            } else {
                qCDebug(lcTlWindowTransition)
                    << "Window transition: originating surface wrapper"
                    << "not found, skipping"
                    << "appId:" << wrapper->appId();
                // Discard the pending rect object.
                m_windowTransitionManagerV1->discardPendingRect(token);
            }
        }
    }

    // Don't use hasActiveCapability() here — it also checks UnMinimized,
    // but minimized windows should be allowed to activate (which unminimizes them).
    if (!wsurface->mapped()) {
        if (disposition == ActivationManagerInterfaceV1::TokenDisposition::Active)
            wrapper->setPendingActivation(seat);
        else
            qCWarning(lcTlCore) << "Activation request for unmapped surface!";
        return;
    }

    if (!wrapper->hasInitializeContainer()) {
        qCWarning(lcTlCore) << "Activation request for surface without initialized container:"
                           << "container =" << wrapper->container();
        return;
    }

    switch (disposition) {
    case ActivationManagerInterfaceV1::TokenDisposition::Active:
        forceActivateSurface(wrapper, Qt::OtherFocusReason, seat);
        break;
    case ActivationManagerInterfaceV1::TokenDisposition::Attention:
        wrapper->setAttention(true);
        break;
    case ActivationManagerInterfaceV1::TokenDisposition::Invalid:
        // Use a relaxed policy: fallback to attention if the token is invalid
        wrapper->setAttention(true);
        break;
    }
}
