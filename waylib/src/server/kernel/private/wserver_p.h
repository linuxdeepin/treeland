// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wserver.h"
#include "wglobal_p.h"

struct wl_event_loop;

QT_BEGIN_NAMESPACE
class QSocketNotifier;
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_display;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WServerPrivate : public WObjectPrivate
{
public:
    WServerPrivate(WServer *qq);
    ~WServerPrivate();

    void init();
    void stop();

    void initSocket(WSocket *socketServer);

    W_DECLARE_PUBLIC(WServer)
    std::unique_ptr<QSocketNotifier> sockNot;

    QVector<WServerInterface*> interfaceList;
    WServerInterface *pendingInterface = nullptr;

    std::unique_ptr<QW_NAMESPACE::qw_display> display;
    wl_event_loop *loop = nullptr;

    QList<WSocket*> sockets;

    GlobalFilterFunc globalFilterFunc = nullptr;
    void *globalFilterFuncData = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
