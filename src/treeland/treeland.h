// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>
#include <wsocket.h>

#include <QDBusContext>
#include <QDBusUnixFileDescriptor>

#include <memory>

class QLocalSocket;
class Helper;

namespace Treeland {

class Treeland
    : public QObject
    , protected QDBusContext
{
    Q_OBJECT

public:
    explicit Treeland(Helper *helper);

    Q_INVOKABLE void retranslate() noexcept;

    bool testMode() const;

    bool debugMode() const;

Q_SIGNALS:
    void socketDisconnected();

public Q_SLOTS:
    bool ActivateWayland(QDBusUnixFileDescriptor fd);
    QString XWaylandName();

private Q_SLOTS:
    void connected();
    void disconnected();
    void readyRead();
    void error();

private:
    QLocalSocket *m_socket{ nullptr };
    QLocalSocket *m_helperSocket{ nullptr };
    Helper *m_helper{ nullptr };
    QMap<QString, std::shared_ptr<WAYLIB_SERVER_NAMESPACE::WSocket>> m_userWaylandSocket;
    QMap<QString, std::shared_ptr<QDBusUnixFileDescriptor>> m_userDisplayFds;
};
} // namespace Treeland
