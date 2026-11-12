// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sessionseatmanager.h"
#include "session.h"
#include "sessioninputchannel.h"
#include "desktop/desktopenvironment.h"
#include "input/compositorinputfilter.h"
#include "core/waylandserver.h"
#include "core/shellhandler.h"
#include "core/imcandidatepanelmanager.h"
#include "seat/seatmanager.h"
#include <wseat.h>
#include <wpointergesturesv1.h>
#include <winputmethodhelper.h>
#include <winputmethodv2_p.h>
#include <wtextinputv1_p.h>
#include <wtextinputv2_p.h>
#include <wtextinputv3_p.h>
#include <wvirtualkeyboardv1_p.h>

WAYLIB_SERVER_USE_NAMESPACE

SessionSeatManager::SessionSeatManager(Session *session)
    : SessionComponent(session)
{
    m_pointerGestures = new WPointerGesturesV1;
    session->attachInterface(m_pointerGestures);
    createInputMethodProtocols();
    auto *hardware = WaylandServer::instance()->seatManager();
    const auto names = hardware->configuredSeatNames();
    for (const auto &name : names) {
        // Hardware topology survives logout; never derive identity from the
        // lifetime of the previously active user's logical seats.
        auto *bootstrap = hardware->getSeat(name);
        auto *logical = bootstrap && !bootstrap->ownsSocket() ? bootstrap : new WSeat(name);
        addSeat(logical, name == hardware->configuredFallbackName());
    }
}

SessionSeatManager::~SessionSeatManager()
{
    delete m_candidatePanels;
    qDeleteAll(m_helpers);
    if (auto *server = WaylandServer::instance())
        server->seatManager()->releaseSessionSeats(this);
}

QList<WSeat *> SessionSeatManager::seats() const
{
    QList<WSeat *> result;
    for (const auto &seat : m_seats) {
        if (seat)
            result.append(seat);
    }
    return result;
}

WSeat *SessionSeatManager::fallbackSeat() const { return m_fallback; }
WInputMethodHelper *SessionSeatManager::inputMethodHelper(WSeat *seat) const
{
    return m_helpers.value(seat);
}
IMCandidatePanelManager *SessionSeatManager::candidatePanels() const { return m_candidatePanels; }

void SessionSeatManager::addSeat(WSeat *seat, bool fallback)
{
    if (m_seats.contains(seat))
        return;
    seat->setPointerGestures(m_pointerGestures);
    seat->setEventFilter(nullptr);
    seat->setEventFilter(session()->desktop()->inputChannel()->filter());
    session()->attachInterface(seat);
    m_seats.append(seat);
    if (fallback || !m_fallback)
        m_fallback = seat;
    auto *helper = new WInputMethodHelper(
        WaylandServer::instance()->server(), seat, m_inputMethodProtocols);
    helper->setParent(this);
    WaylandServer::instance()->shellHandler()->registerInputMethodHelper(helper);
    m_helpers.insert(seat, helper);
    if (!m_candidatePanels)
        m_candidatePanels = new IMCandidatePanelManager(
            WaylandServer::instance()->shellHandler(), helper, this);
    else if (fallback)
        m_candidatePanels->setInputMethodHelper(helper);
}

void SessionSeatManager::removeSeat(WSeat *seat)
{
    delete m_helpers.take(seat);
    if (m_candidatePanels)
        m_candidatePanels->setInputMethodHelper(m_helpers.isEmpty() ? nullptr : m_helpers.first());
    m_seats.removeAll(seat);
    if (m_fallback == seat)
        m_fallback = m_seats.isEmpty() ? nullptr : m_seats.first();
    session()->detachInterface(seat);
}

void SessionSeatManager::createInputMethodProtocols()
{
    m_inputMethodProtocols = {
        new WInputMethodManagerV2,
        new WTextInputManagerV1,
        new WTextInputManagerV2,
        new WTextInputManagerV3,
        new WVirtualKeyboardManagerV1,
    };
    session()->attachInterface(m_inputMethodProtocols.inputMethod);
    session()->attachInterface(m_inputMethodProtocols.textInputV1);
    session()->attachInterface(m_inputMethodProtocols.textInputV2);
    session()->attachInterface(m_inputMethodProtocols.textInputV3);
    session()->attachInterface(m_inputMethodProtocols.virtualKeyboard);
}
