// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sessioncomponent.h"
#include "session.h"

SessionComponent::SessionComponent(Session *session)
    : QObject(session)
    , m_session(session)
{
    Q_ASSERT(session);
    connect(session, &Session::activeChanged, this, &SessionComponent::setActive);
}

Session *SessionComponent::session() const
{
    return m_session;
}

bool SessionComponent::isActive() const
{
    return m_active;
}

void SessionComponent::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    if (active)
        activate();
    else
        deactivate();
}

void SessionComponent::initializeActiveState()
{
    setActive(m_session->isActive());
}

void SessionComponent::activate() {}
void SessionComponent::deactivate() {}
