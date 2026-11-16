// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "sessioncomponent.h"

#include <wglobal.h>
#include <winputmethodhelper.h>

#include <QList>
#include <QMap>
#include <QPointer>

class IMCandidatePanelManager;
WAYLIB_SERVER_BEGIN_NAMESPACE
class WSeat;
class WPointerGesturesV1;
WAYLIB_SERVER_END_NAMESPACE

class SessionSeatManager : public SessionComponent
{
    Q_OBJECT
public:
    explicit SessionSeatManager(Session *session);
    ~SessionSeatManager() override;

    QList<WAYLIB_SERVER_NAMESPACE::WSeat *> seats() const;
    WAYLIB_SERVER_NAMESPACE::WSeat *fallbackSeat() const;
    WAYLIB_SERVER_NAMESPACE::WInputMethodHelper *inputMethodHelper(
        WAYLIB_SERVER_NAMESPACE::WSeat *seat) const;
    IMCandidatePanelManager *candidatePanels() const { return m_candidatePanels; };
    bool hasInputMethod() const;
    void addSeat(WAYLIB_SERVER_NAMESPACE::WSeat *seat, bool fallback);
    void removeSeat(WAYLIB_SERVER_NAMESPACE::WSeat *seat);

Q_SIGNALS:
    void inputMethodChanged(bool connected);

protected:
    void activate() override;
    void deactivate() override;

private:
    void createInputMethodProtocols();
    void updateInputMethodState();
    bool m_inputMethodConnected = false;
    WAYLIB_SERVER_NAMESPACE::WPointerGesturesV1 *m_pointerGestures = nullptr;
    WAYLIB_SERVER_NAMESPACE::WInputMethodHelper::Protocols m_inputMethodProtocols;
    QList<QPointer<WAYLIB_SERVER_NAMESPACE::WSeat>> m_seats;
    QPointer<WAYLIB_SERVER_NAMESPACE::WSeat> m_fallback;
    QMap<WAYLIB_SERVER_NAMESPACE::WSeat *, WAYLIB_SERVER_NAMESPACE::WInputMethodHelper *> m_helpers;
    IMCandidatePanelManager *m_candidatePanels = nullptr;
};
