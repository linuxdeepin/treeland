// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wserver.h"
#include "wglobal_p.h"
#include "wpointer.h"

struct wl_event_loop;
struct wl_display;
void wl_display_destroy(struct wl_display *display);

QT_BEGIN_NAMESPACE
class QSocketNotifier;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
template <>
struct WlrObjectTraits<wl_display> {
    static void destroy(wl_display *display) { wl_display_destroy(display); }
};

class Q_DECL_HIDDEN WServerPrivate : public WObjectPrivate
{
public:
    WServerPrivate(WServer *qq);
    ~WServerPrivate();

    void init();
    void stop();

    void initSocket(WSocket *socketServer);
    void processWaylandEvents();
    void onAboutToBlock();
    void onAwake();

    W_DECLARE_PUBLIC(WServer)
    std::unique_ptr<QSocketNotifier> sockNot;

    QVector<WServerInterface*> interfaceList;
    WServerInterface *pendingInterface = nullptr;

    WUniquePointer<wl_display> display;
    wl_event_loop *loop = nullptr;

    QList<WSocket*> sockets;

    GlobalFilterFunc globalFilterFunc = nullptr;
    void *globalFilterFuncData = nullptr;

    bool isProcessingEvents = false;
    void safeFlushClients();
};

WAYLIB_SERVER_END_NAMESPACE
