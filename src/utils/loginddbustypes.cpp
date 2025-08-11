// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "loginddbustypes.h"
#include "common/treelandlogging.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QLoggingCategory>

class LogindPathInternal
{
public:
    LogindPathInternal();
    bool available = false;
    QString serviceName;
    QString managerPath;
    QString managerIfaceName;
    QString sessionIfaceName;
    QString seatIfaceName;
    QString userIfaceName;
};

LogindPathInternal::LogindPathInternal()
{
    qRegisterMetaType<NamedSeatPath>("NamedSeatPath");
    qDBusRegisterMetaType<NamedSeatPath>();

    qRegisterMetaType<NamedSeatPathList>("NamedSeatPathList");
    qDBusRegisterMetaType<NamedSeatPathList>();

    qRegisterMetaType<NamedSessionPath>("NamedSessionPath");
    qDBusRegisterMetaType<NamedSessionPath>();

    qRegisterMetaType<NamedSessionPathList>("NamedSessionPathList");
    qDBusRegisterMetaType<NamedSessionPathList>();

    qRegisterMetaType<SessionInfo>("SessionInfo");
    qDBusRegisterMetaType<SessionInfo>();

    qRegisterMetaType<SessionInfoList>("SessionInfoList");
    qDBusRegisterMetaType<SessionInfoList>();

    qRegisterMetaType<UserInfo>("UserInfo");
    qDBusRegisterMetaType<UserInfo>();

    qRegisterMetaType<UserInfoList>("UserInfoList");
    qDBusRegisterMetaType<UserInfoList>();

    if (QDBusConnection::systemBus().interface()->isServiceRegistered(
            QStringLiteral("org.freedesktop.login1"))) {
        qCDebug(treelandDBus) << "Logind interface found";
        available = true;
        serviceName = QStringLiteral("org.freedesktop.login1");
        managerPath = QStringLiteral("/org/freedesktop/login1");
        managerIfaceName = QStringLiteral("org.freedesktop.login1.Manager");
        seatIfaceName = QStringLiteral("org.freedesktop.login1.Seat");
        sessionIfaceName = QStringLiteral("org.freedesktop.login1.Session");
        userIfaceName = QStringLiteral("org.freedesktop.login1.User");
        return;
    }

    if (QDBusConnection::systemBus().interface()->isServiceRegistered(
            QStringLiteral("org.freedesktop.ConsoleKit"))) {
        qCDebug(treelandDBus) << "Console kit interface found";
        available = true;
        serviceName = QStringLiteral("org.freedesktop.ConsoleKit");
        managerPath = QStringLiteral("/org/freedesktop/ConsoleKit/Manager");
        managerIfaceName =
            QStringLiteral("org.freedesktop.ConsoleKit.Manager"); // note this doesn't match logind
        seatIfaceName = QStringLiteral("org.freedesktop.ConsoleKit.Seat");
        sessionIfaceName = QStringLiteral("org.freedesktop.ConsoleKit.Session");
        userIfaceName = QStringLiteral("org.freedesktop.ConsoleKit.User");
        return;
    }
    qCDebug(treelandDBus) << "No session manager found";
}

Q_GLOBAL_STATIC(LogindPathInternal, s_instance);

bool Logind::isAvailable()
{
    return s_instance->available;
}

QString Logind::serviceName()
{
    return s_instance->serviceName;
}

QString Logind::managerPath()
{
    return s_instance->managerPath;
}

QString Logind::managerIfaceName()
{
    return s_instance->managerIfaceName;
}

QString Logind::seatIfaceName()
{
    return s_instance->seatIfaceName;
}

QString Logind::sessionIfaceName()
{
    return s_instance->sessionIfaceName;
}

QString Logind::userIfaceName()
{
    return s_instance->userIfaceName;
}
