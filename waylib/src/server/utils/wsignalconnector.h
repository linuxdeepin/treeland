/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

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
