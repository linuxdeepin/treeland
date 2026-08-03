// Copyright (C) 2022-2026 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>
#include <QVector>

#include <wayland-server-core.h>
#include <functional>

QW_BEGIN_NAMESPACE

class qw_signal_connector
{
    using SlotFun0 = void (*)(void *obj);
    using SlotFun1 = void (*)(void *obj, void *signalData);
    using SlotFun2 = void (*)(void *obj, void *signalData, void *data);

    struct qw_signal_listener {
        wl_signal *signal;
        wl_listener l;
        void *object;
        union {
            SlotFun0 slot0;
            SlotFun1 slot1;
            SlotFun2 slot2;
        };
        void *data = nullptr;
        // Non-null only for member-function-pointer connects.
        // Heap-allocated to keep qw_signal_listener standard-layout so that
        // wl_container_of (which uses offsetof) remains well-defined.
        // Receives (wl signal data, user extra data); both may be null.
        std::function<void(void *, void *)> *mfp_fn = nullptr;

        ~qw_signal_listener() { delete mfp_fn; }
    };

public:
    qw_signal_connector() {
        listenerList.reserve(1);
    }

    ~qw_signal_connector() {
        invalidate();
    }


    qw_signal_listener *connect(wl_signal *signal, void *object, SlotFun0 slot) {
        qw_signal_listener *l = new qw_signal_listener;
        listenerList.push_back(l);

        l->signal = signal;
        l->l.notify = callSlot0;
        l->object = object;
        l->slot0 = slot;
        wl_signal_add(signal, &l->l);
        return l;
    }

    qw_signal_listener *connect(wl_signal *signal, void *object, SlotFun1 slot) {
        qw_signal_listener *l = new qw_signal_listener;
        listenerList.push_back(l);

        l->signal = signal;
        l->l.notify = callSlot1;
        l->object = object;
        l->slot1 = slot;
        wl_signal_add(signal, &l->l);
        return l;
    }

    qw_signal_listener *connect(wl_signal *signal, void *object, SlotFun2 slot, void *data) {
        Q_ASSERT(data);
        qw_signal_listener *l = new qw_signal_listener;
        listenerList.push_back(l);

        l->signal = signal;
        l->l.notify = callSlot2;
        l->object = object;
        l->slot2 = slot;
        l->data = data;
        wl_signal_add(signal, &l->l);
        return l;
    }
    template <typename T>
    inline qw_signal_listener *connect(wl_signal *signal, T *object, void (*slot)(T*)) {
        return connect(signal, object, reinterpret_cast<SlotFun0>(slot));
    }
    template <typename T>
    inline qw_signal_listener *connect(wl_signal *signal, T *object, void (*slot)(T*))
        requires ( !std::is_same_v<void,T> ) {
        return connect(signal, object, reinterpret_cast<SlotFun0>(slot));
    }
    template <typename T, typename T1>
    inline qw_signal_listener *connect_t(wl_signal *signal, T *object, void (*slot)(T*, T1*))
        requires ( !std::is_same_v<void,T> ) {
        return connect(signal, object, reinterpret_cast<SlotFun1>(slot));
    }
    template <typename T, typename T1, typename T2, typename T3>
    inline qw_signal_listener *connect(wl_signal *signal, T *object, void (*slot)(T*, T1*, T2*), T3 *data)
        requires ( !std::is_same_v<void,T> ) {
        return connect(signal, object, reinterpret_cast<SlotFun2>(slot), data);
    }
    // Member-function-pointer overloads.
    //
    // Member function pointers cannot be converted to regular function pointers:
    // an MFP may carry a this-pointer adjustment and, for virtual functions,
    // dispatches through the vtable rather than holding a direct function address.
    // They are therefore semantically incompatible with free function pointers,
    // and any cast between the two types is undefined behavior.
    //
    // Instead we capture the object pointer and MFP in a std::function lambda.
    // The lambda is stored in qw_signal_listener::mfp_fn and invoked by the
    // shared callMfp notify callback.  No type-punning involved.
    template <typename T, typename TSlot>
    inline qw_signal_listener *connect(wl_signal *signal, T *object, void (TSlot::*slot)())
        requires ( std::is_base_of_v<TSlot,T> ) {
        auto *typed_obj = static_cast<TSlot *>(object);
        qw_signal_listener *l = new qw_signal_listener;
        listenerList.push_back(l);
        l->signal = signal;
        l->l.notify = callMfp;
        l->mfp_fn = new std::function<void(void *, void *)>([typed_obj, slot](void *, void *) {
            (typed_obj->*slot)();
        });
        wl_signal_add(signal, &l->l);
        return l;
    }
    template <typename T, typename T1, typename TSlot>
    inline qw_signal_listener *connect(wl_signal *signal, T *object, void (TSlot::*slot)(T1*))
        requires ( std::is_base_of_v<TSlot,T> ) {
        auto *typed_obj = static_cast<TSlot *>(object);
        qw_signal_listener *l = new qw_signal_listener;
        listenerList.push_back(l);
        l->signal = signal;
        l->l.notify = callMfp;
        l->mfp_fn = new std::function<void(void *, void *)>([typed_obj, slot](void *sigdata, void *) {
            (typed_obj->*slot)(static_cast<T1 *>(sigdata));
        });
        wl_signal_add(signal, &l->l);
        return l;
    }
    template <typename T, typename T1, typename T2, typename T3, typename TSlot>
    inline qw_signal_listener *connect(wl_signal *signal, T *object, void (TSlot::*slot)(T1*, T2*), T3 *data)
        requires ( std::is_base_of_v<TSlot,T> ) {
        auto *typed_obj = static_cast<TSlot *>(object);
        qw_signal_listener *l = new qw_signal_listener;
        listenerList.push_back(l);
        l->signal = signal;
        l->l.notify = callMfp;
        l->data = static_cast<void *>(data);
        l->mfp_fn = new std::function<void(void *, void *)>([typed_obj, slot](void *sigdata, void *extra) {
            (typed_obj->*slot)(static_cast<T1 *>(sigdata), static_cast<T2 *>(extra));
        });
        wl_signal_add(signal, &l->l);
        return l;
    }
    void disconnect(qw_signal_listener *l) {
        Q_ASSERT(listenerList.contains(l));
        wl_list_remove(&l->l.link);
        delete l;
        listenerList.removeOne(l);
    }
    void disconnect(wl_signal *signal) {
        auto tmpList = listenerList;
        auto begin = tmpList.constBegin();
        while (begin != tmpList.constEnd()) {
            qw_signal_listener *l = *begin;
            ++begin;

            if (signal == l->signal)
                disconnect(l);
        }
    }
    void invalidate() {
        QVector<qw_signal_listener*> tmpList;
        tmpList.swap(listenerList);
        auto begin = tmpList.constBegin();
        while (begin != tmpList.constEnd()) {
            qw_signal_listener *l = *begin;
            wl_list_remove(&l->l.link);
            ++begin;
            delete l;
        }
    }

private:
    static void callSlot0(wl_listener *wl_listener, void *) {
        qw_signal_listener *listener = wl_container_of(wl_listener, listener, l);
        listener->slot0(listener->object);
    }

    static void callSlot1(wl_listener *wl_listener, void *data) {
        qw_signal_listener *listener = wl_container_of(wl_listener, listener, l);
        listener->slot1(listener->object, data);
    }

    static void callSlot2(wl_listener *wl_listener, void *data) {
        qw_signal_listener *listener = wl_container_of(wl_listener, listener, l);
        listener->slot2(listener->object, data, listener->data);
    }

    static void callMfp(wl_listener *wl_listener, void *data) {
        qw_signal_listener *listener = wl_container_of(wl_listener, listener, l);
        (*listener->mfp_fn)(data, listener->data);
    }

    QVector<qw_signal_listener*> listenerList;
};

QW_END_NAMESPACE
