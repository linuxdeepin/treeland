// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "shortcutmodel.h"

#include <wserver.h>

#include <QObject>
#include <QQmlEngine>

class treeland_shortcut_context_v1;
class treeland_shortcut_manager_v1;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class ShortcutV1
    : public QObject
    , public Waylib::Server::WServerInterface
{
    Q_OBJECT
    Q_PROPERTY(ShortcutModel *model READ model CONSTANT)

public:
    explicit ShortcutV1(QObject *parent = nullptr);
    QByteArrayView interfaceName() const override;

    ShortcutModel *model() const { return m_model; }

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private Q_SLOTS:
    void onNewContext(uid_t uid, treeland_shortcut_context_v1 *context);

private:
    treeland_shortcut_manager_v1 *m_manager = nullptr;
    ShortcutModel *m_model = nullptr;
};
