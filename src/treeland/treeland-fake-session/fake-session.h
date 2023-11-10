// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QGuiApplication>
#include <QtWaylandClient/QWaylandClientExtension>

#include "qwayland-ext-foreign-toplevel-list-v1.h"
#include "qwayland-ztreeland-foreign-toplevel-manager-v1.h"
#include "qwayland-ztreeland-shortcut-manager-v1.h"

class ExtForeignToplevelHandle;
class ExtForeignToplevelList : public QWaylandClientExtensionTemplate<ExtForeignToplevelList>, public QtWayland::ext_foreign_toplevel_list_v1
{
    Q_OBJECT
public:
    explicit ExtForeignToplevelList();

Q_SIGNALS:
    void newToplevel(ExtForeignToplevelHandle *handle);

protected:
    virtual void ext_foreign_toplevel_list_v1_toplevel(struct ::ext_foreign_toplevel_handle_v1 *toplevel);
    virtual void ext_foreign_toplevel_list_v1_finished();
};

class ExtForeignToplevelHandle : public QWaylandClientExtensionTemplate<ExtForeignToplevelHandle>, public QtWayland::ext_foreign_toplevel_handle_v1
{
    Q_OBJECT
public:
    explicit ExtForeignToplevelHandle(struct ::ext_foreign_toplevel_handle_v1 *object);

Q_SIGNALS:
    void appIdChanged(const QString &appId);

protected:
    void ext_foreign_toplevel_handle_v1_app_id(const QString &app_id) override;
    void ext_foreign_toplevel_handle_v1_closed() override;
    void ext_foreign_toplevel_handle_v1_identifier(const QString &identifier) override;
};

class ForeignToplevelHandle;
class ForeignToplevelManager : public QWaylandClientExtensionTemplate<ForeignToplevelManager>, public QtWayland::ztreeland_foreign_toplevel_manager_v1
{
    Q_OBJECT
public:
    explicit ForeignToplevelManager();

Q_SIGNALS:
    void newForeignToplevelHandle(ForeignToplevelHandle *handle);

protected:
    void ztreeland_foreign_toplevel_manager_v1_toplevel(struct ::ztreeland_foreign_toplevel_handle_v1 *toplevel) override;
};

class ForeignToplevelHandle : public QWaylandClientExtensionTemplate<ForeignToplevelHandle>, public QtWayland::ztreeland_foreign_toplevel_handle_v1
{
    Q_OBJECT
public:
    explicit ForeignToplevelHandle(struct ::ztreeland_foreign_toplevel_handle_v1 *object);

Q_SIGNALS:
    void pidChanged(uint32_t pid);

protected:
    void ztreeland_foreign_toplevel_handle_v1_app_id(const QString &app_id) override;
    void ztreeland_foreign_toplevel_handle_v1_pid(uint32_t pid) override;
    void ztreeland_foreign_toplevel_handle_v1_done() override;
    void ztreeland_foreign_toplevel_handle_v1_closed() override;
    void ztreeland_foreign_toplevel_handle_v1_identifier(const QString &identifier) override;

private:
    uint32_t m_pid;
};

class ShortcutManager : public QWaylandClientExtensionTemplate<ShortcutManager>, public QtWayland::ztreeland_shortcut_manager_v1
{
    Q_OBJECT
public:
    explicit ShortcutManager();
};

class ShortcutContext : public QWaylandClientExtensionTemplate<ShortcutContext>, public QtWayland::ztreeland_shortcut_context_v1
{
    Q_OBJECT
public:
    explicit ShortcutContext(struct ::ztreeland_shortcut_context_v1 *object);

Q_SIGNALS:
    void shortcutHappended(uint32_t keycode, uint32_t modify);

protected:
    void ztreeland_shortcut_context_v1_shortcut(uint32_t keycode, uint32_t modify) override;
};

class FakeSession : public QGuiApplication {
    Q_OBJECT
public:
    explicit FakeSession(int argc, char* argv[]);

private:
    ShortcutManager* m_shortcutManager;
    ForeignToplevelManager *m_toplevelManager;
    ExtForeignToplevelList *m_extForeignToplevelList;
};
