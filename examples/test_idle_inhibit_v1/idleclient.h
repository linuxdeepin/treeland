// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "idleinhibitv1.h"
#include "idlenotifyv1.h"

#include <QObject>

class QWidget;
class QTimer;

class IdleClient : public QObject
{
    Q_OBJECT
public:
    explicit IdleClient(QObject *parent = nullptr);
    ~IdleClient();

    bool initialize(uint32_t idleTimeout,
                    uint32_t inhibitDuration,
                    uint32_t dbusInhibitDuration,
                    const QString &execCommand);

    bool isValid() const;

Q_SIGNALS:
    void idleStateChanged(bool idle);

private Q_SLOTS:
    void onIdled();
    void onResumed();

private:
    void setupInhibit();
    void releaseInhibit();
    void setupDBusInhibit();
    void releaseDBusInhibit();
    void executeCommand();

private:
    std::unique_ptr<IdleNotifierV1> m_notifier;
    std::unique_ptr<IdleNotificationV1> m_notification;
    std::unique_ptr<IdleInhibitManagerV1> m_inhibitManager;
    std::unique_ptr<IdleInhibitorV1> m_inhibitor;

    uint32_t m_idleTimeout = 0;
    uint32_t m_inhibitDuration = 0;
    uint32_t m_dbusInhibitDuration = 0;
    bool m_isIdle = false;
    bool m_inhibitActive = false;

    QString m_execCommand;
    QTimer *m_inhibitTimer = nullptr;
    QWidget *m_inhibitWindow = nullptr;
    uint32_t m_dbusInhibitCookie = 0;
};
