// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-treeland-shortcut-manager-v2.h"

#include <QApplication>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QtWaylandClient/QWaylandClientExtension>

class Shortcut;

class ShortcutV2
    : public QWaylandClientExtensionTemplate<ShortcutV2>
    , public QtWayland::treeland_shortcut_manager_v2
{
    Q_OBJECT
public:
    explicit ShortcutV2();
Q_SIGNALS:
    void commitStatus(bool success);
    void activated(const QString &name, uint32_t repeat);
protected:
    void treeland_shortcut_manager_v2_commit_success() override;
    void treeland_shortcut_manager_v2_commit_failure(const QString &name, uint32_t error) override;
    void treeland_shortcut_manager_v2_activated(const QString &name, uint32_t repeat) override;
};

class Shortcut
    : public QObject
{
    Q_OBJECT
public:
    explicit Shortcut(const QString &path, const QString &name);

    ~Shortcut() override;

    void exec();
    void registerForManager(ShortcutV2 *manager);

Q_SIGNALS:
    void before_destroy();

private:
    QSettings m_settings;
    QString m_shortcutName;
    QList<QString> m_registeredBindings;
};
