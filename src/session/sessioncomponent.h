// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QPointer>
#include <QtQml/qqmlregistration.h>

class Session;

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
    void initializeActiveState();
    virtual void activate();
    virtual void deactivate();

private:
    void setActive(bool active);
    QPointer<Session> m_session;
    bool m_active = false;
};
