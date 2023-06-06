// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsignalconnector.h"

#include <wayland-server-core.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

struct Listener {
    wl_signal *signal;
    wl_listener l;
    void *object;
    union {
        WSignalConnector::SlotFun0 slot0;
        WSignalConnector::SlotFun1 slot1;
        WSignalConnector::SlotFun2 slot2;
    };
    void *data;
};

static void callSlot0(wl_listener *wl_listener, void *) {
    Listener *listener = wl_container_of(wl_listener, listener, l);
    listener->slot0(listener->object);
}

static void callSlot1(wl_listener *wl_listener, void *data) {
    Listener *listener = wl_container_of(wl_listener, listener, l);
    listener->slot1(listener->object, data);
}

static void callSlot2(wl_listener *wl_listener, void *data) {
    Listener *listener = wl_container_of(wl_listener, listener, l);
    listener->slot2(listener->object, data, listener->data);
}

WSignalConnector::WSignalConnector()
{
    listenerList.reserve(1);
}

WSignalConnector::~WSignalConnector()
{
    auto begin = listenerList.begin();
    while (begin != listenerList.end()) {
        Listener *l = *begin;
        wl_list_remove(&l->l.link);
        delete l;
        ++begin;
    }
}

Listener *WSignalConnector::connect(wl_signal *signal, void *object, SlotFun0 slot)
{
    Listener *l = new Listener;
    listenerList.push_back(l);

    l->signal = signal;
    l->l.notify = callSlot0;
    l->object = object;
    l->slot0 = slot;
    wl_signal_add(signal, &l->l);
    return l;
}

Listener *WSignalConnector::connect(wl_signal *signal, void *object, SlotFun1 slot)
{
    Listener *l = new Listener;
    listenerList.push_back(l);

    l->signal = signal;
    l->l.notify = callSlot1;
    l->object = object;
    l->slot1 = slot;
    wl_signal_add(signal, &l->l);
    return l;
}

Listener *WSignalConnector::connect(wl_signal *signal, void *object, SlotFun2 slot, void *data)
{
    Q_ASSERT(data);
    Listener *l = new Listener;
    listenerList.push_back(l);

    l->signal = signal;
    l->l.notify = callSlot2;
    l->object = object;
    l->slot2 = slot;
    l->data = data;
    wl_signal_add(signal, &l->l);
    return l;
}

void WSignalConnector::disconnect(Listener *l)
{
    Q_ASSERT(listenerList.contains(l));
    wl_list_remove(&l->l.link);
    delete l;
    listenerList.removeOne(l);
}

void WSignalConnector::disconnect(wl_signal *signal)
{
    auto tmpList = listenerList;
    auto begin = tmpList.begin();
    while (begin != tmpList.end()) {
        Listener *l = *begin;
        ++begin;

        if (signal == l->signal)
            disconnect(l);
    }
}

// In this function will not remove the wl_list, use in the destroy
// the corresponding wlr object before.
void WSignalConnector::invalidate()
{
    auto tmpList = listenerList;
    listenerList.clear();
    auto begin = tmpList.begin();
    while (begin != tmpList.end()) {
        Listener *l = *begin;
        ++begin;
        delete l;
    }
}

void WSignalConnector::invalidate(wl_signal *signal)
{
    auto tmpList = listenerList;
    auto begin = tmpList.begin();
    while (begin != tmpList.end()) {
        Listener *l = *begin;
        ++begin;

        if (signal == l->signal) {
            delete l;
            listenerList.removeOne(l);
        }
    }
}

WAYLIB_SERVER_END_NAMESPACE
