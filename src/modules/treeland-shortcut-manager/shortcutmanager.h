// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "helper.h"

#include <wquickwaylandserver.h>

#include <QObject>
#include <QQmlEngine>

class treeland_shortcut_context_v1;
class treeland_shortcut_manager_v1;

class ShortcutManagerV1 : public Waylib::Server::WQuickWaylandServerInterface
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ShortcutManager)
    Q_PROPERTY(Helper *helper READ helper WRITE setHelper)

public:
    explicit ShortcutManagerV1(QObject *parent = nullptr);

protected:
    WServerInterface *create() override;

private Q_SLOTS:
    void onNewContext(uid_t uid, treeland_shortcut_context_v1 *context);

private:
    void setHelper(Helper *helper);
    Helper *helper();

    treeland_shortcut_manager_v1 *m_manager = nullptr;
    Helper *m_helper = nullptr;
};
