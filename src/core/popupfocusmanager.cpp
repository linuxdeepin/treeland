// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "popupfocusmanager.h"

#include "common/treelandlogging.h"
#include "core/rootsurfacecontainer.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <wseat.h>
#include <wxdgpopupsurface.h>

#include <qwseat.h>
#include <qwxdgshell.h>

WAYLIB_SERVER_USE_NAMESPACE

PopupFocusManager::PopupFocusManager(WSeat *seat, QObject *parent)
    : QObject(parent)
    , m_seat(seat)
{
    auto *seatHandle = seat->handle();
    connect(seatHandle,
            &qw_seat::notify_keyboard_grab_begin,
            this,
            &PopupFocusManager::onKeyboardGrabBegin);
    connect(seatHandle,
            &qw_seat::notify_keyboard_grab_end,
            this,
            &PopupFocusManager::onKeyboardGrabEnd);
}

PopupFocusManager::~PopupFocusManager() = default;

void PopupFocusManager::giveFocus(WXdgPopupSurface *popupSurface)
{
    if (!m_hasPopupGrab || !popupSurface || !m_seat->nativeHandle())
        return;

    // Only give focus to popups that belong to our seat's active popup grab.
    // In wlroots, wlr_xdg_popup->seat is set when the popup is added to
    // a seat's popup_grab->popups list and cleared on removal
    auto *wlrPopup = popupSurface->handle()->handle();
    if (!wlrPopup || wlrPopup->seat != m_seat->nativeHandle())
        return;

    // Save the current keyboard focus surface before moving focus to the popup.
    if (!m_savedFocusSurface) {
        if (auto *container =
                Helper::instance()->rootSurfaceContainer()->getSeatContainerOrDefault(m_seat))
            m_savedFocusSurface = container->keyboardFocusSurface();
    }

    // Move keyboard focus to the popup surface via the normal waylib path.
    // TODO(rewine): Support multi-seat
    m_seat->setKeyboardFocusSurface(popupSurface->surface());

    qCDebug(lcTlPopupFocus) << "Moved keyboard focus to popup surface:" << popupSurface;
}

void PopupFocusManager::dismissAll()
{
    if (!m_hasPopupGrab)
        return;

    qCDebug(lcTlPopupFocus) << "Dismissing all popup surfaces";
    Q_EMIT aboutToDismissAll();
}

void PopupFocusManager::onKeyboardGrabBegin()
{
    // TODO(rewine): Only track xdg_popup keyboard grabs — ignore IME and other compositor grabs.
    // Iterate wlr_xdg_shell.popup_grabs and compare seat->keyboard_state.grab ==
    // &popup_grab->keyboard_grab to precisely identify popup grabs.
    auto *wlrSeat = m_seat->nativeHandle();
    if (wlrSeat && wlrSeat->drag)
        return;

    if (m_hasPopupGrab) {
        // Already tracking; nested popups reuse the same grab.
        return;
    }
    m_hasPopupGrab = true;
    qCDebug(lcTlPopupFocus) << "Popup keyboard grab started";
}

void PopupFocusManager::onKeyboardGrabEnd()
{
    if (!m_hasPopupGrab)
        return;

    m_hasPopupGrab = false;

    qCDebug(lcTlPopupFocus) << "Popup keyboard grab ended, restoring focus to:"
                            << m_savedFocusSurface;

    auto saved = m_savedFocusSurface;
    m_savedFocusSurface = nullptr;

    if (saved && saved->hasFocusCapability()) {
        // TODO(rewine): Only restore focus if the saved surface belongs to the same seat.
        Helper::instance()->requestKeyboardFocus(saved, Qt::ActiveWindowFocusReason);
    }
}
