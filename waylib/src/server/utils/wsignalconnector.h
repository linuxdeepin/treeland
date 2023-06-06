// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QVector>

struct wl_signal;

WAYLIB_SERVER_BEGIN_NAMESPACE

struct Listener;
class WSignalConnector {
public:
    WSignalConnector();
    ~WSignalConnector();

    typedef void (*SlotFun0)(void *obj);
    Listener *connect(wl_signal *signal, void *object, SlotFun0 slot);
    typedef void (*SlotFun1)(void *obj, void *signalData);
    Listener *connect(wl_signal *signal, void *object, SlotFun1 slot);
    typedef void (*SlotFun2)(void *obj, void *signalData, void *data);
    Listener *connect(wl_signal *signal, void *object, SlotFun2 slot, void *data);
    template <typename T>
    inline Listener *connect(wl_signal *signal, T *object, void (T::*slot)()) {
        return connect(signal, object, reinterpret_cast<SlotFun0>(*(void**)(&slot)));
    }
    template <typename T>
    inline Listener *connect(wl_signal *signal, T *object, void (T::*slot)(void*)) {
        return connect(signal, object, reinterpret_cast<SlotFun1>(*(void**)(&slot)));
    }
    template <typename T>
    inline Listener *connect(wl_signal *signal, T *object, void (T::*slot)(void*, void*), void *data) {
        return connect(signal, object, reinterpret_cast<SlotFun2>(*(void**)(&slot)), data);
    }
    void disconnect(Listener *l);
    void disconnect(wl_signal *signal);
    void invalidate();
    void invalidate(wl_signal *signal);

private:
    QVector<Listener*> listenerList;
};

WAYLIB_SERVER_END_NAMESPACE
