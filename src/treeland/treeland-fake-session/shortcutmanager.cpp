// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcutmanager.h"

#include <QDBusInterface>

Shortcut::Shortcut(const QString &path)
    : m_settings(QSettings(path, QSettings::IniFormat))
{
}

void Shortcut::exec() {
    const QString &type = m_settings.value("Shortcut/Type").toString();

    if (type == "Exec") {
        const QString &exec = m_settings.value("Type.Exec/Exec").toString();
        const QString &args = m_settings.value("Type.Exec/Args").toString();
        QProcess::startDetached(exec, args.split(" "));
    }

    if (type == "DBus") {
        const QString &service = m_settings.value("Type.DBus/Service").toString();
        const QString &path = m_settings.value("Type.DBus/Path").toString();
        const QString &interface = m_settings.value("Type.DBus/Interface").toString();
        const QString &method = m_settings.value("Type.DBus/Method").toString();

        QDBusInterface dbus(service, path, interface);
        dbus.asyncCall(method);
    }

    if (type == "Action") {
    }
}

QString Shortcut::shortcut() {
    return m_settings.value("Shortcut/Shortcut").toString();
}
