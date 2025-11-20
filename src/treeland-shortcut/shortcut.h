// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-treeland-shortcut-manager-v1.h"

#include <QApplication>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QtWaylandClient/QWaylandClientExtension>

class ShortcutV1;
class Shortcut;

class ShortcutManagerV1
    : public QWaylandClientExtensionTemplate<ShortcutManagerV1>
    , public QtWayland::treeland_shortcut_manager_v1
{
    Q_OBJECT
public:
    explicit ShortcutManagerV1();

private:
    std::vector<std::unique_ptr<ShortcutV1>> m_customShortcuts;
    std::vector<std::unique_ptr<ShortcutV1>> m_treelandShortcutContexts;
    std::vector<std::unique_ptr<Shortcut>> m_treelandShortcuts;
};

class ShortcutV1
    : public QWaylandClientExtensionTemplate<ShortcutV1>
    , public QtWayland::treeland_shortcut_v1
{
    Q_OBJECT
public:
    explicit ShortcutV1(struct ::treeland_shortcut_v1 *object);
    ~ShortcutV1() override;

Q_SIGNALS:
    void shortcutHappended();

protected:
    void treeland_shortcut_v1_activated() override;
    void treeland_shortcut_v1_bind_success(uint32_t binding_id) override;
    void treeland_shortcut_v1_bind_failure(uint32_t reason) override;
};

class Shortcut
{
public:
    explicit Shortcut(const QString &path);

    virtual ~Shortcut() = default;

    void exec();

    QString shortcut();

private:
    QSettings m_settings;
};
