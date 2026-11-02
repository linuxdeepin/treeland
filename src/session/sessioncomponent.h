// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QPointer>
#include <QtQml/qqmlregistration.h>

class Session;
Q_MOC_INCLUDE("session/session.h")

// User-owned services receive their identity once, at construction.
class SessionComponent : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(Session *session READ session CONSTANT FINAL)
public:
    explicit SessionComponent(Session *session);
    Session *session() const;
    bool isActive() const;

protected:
    // A failed backend uses the generated defaults, rather than retaining the
    // previous user's device state. Pending configs are never applied.
    template<typename Config>
    static bool configurationReady(Config *config)
    {
        return config->isInitializeSucceeded() || config->isInitializeFailed();
    }

    void initializeActiveState();
    virtual void activate();
    virtual void deactivate();

private:
    void setActive(bool active);
    QPointer<Session> m_session;
    bool m_active = false;
};
