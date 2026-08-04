// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "wglobal.h"
#include <qwobject.h>
#include <QPointer>

#include <functional>

extern "C" {
#include <wayland-server-core.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WObjectPrivate
{
public:
    static WObjectPrivate *get(WObject *qq);

    virtual ~WObjectPrivate();
    virtual wl_client *waylandClient() const {
        return nullptr;
    }

protected:
    WObjectPrivate(WObject *qq);

    inline int indexOfAttachedData(const void *owner) const {
        for (int i = 0; i < attachedDatas.count(); ++i)
            if (attachedDatas.at(i).first == owner)
                return i;
        return -1;
    }

    WObject *q_ptr;
    QList<std::pair<const void*, void*>> attachedDatas;

    W_DECLARE_PUBLIC(WObject)
};

class WNativeListener
{
public:
    WNativeListener()
    {
        listener.notify = handleEvent;
        wl_list_init(&listener.link);
    }

    ~WNativeListener()
    {
        disconnect();
    }

    WNativeListener(const WNativeListener &) = delete;
    WNativeListener &operator=(const WNativeListener &) = delete;
    WNativeListener(WNativeListener &&) = delete;
    WNativeListener &operator=(WNativeListener &&) = delete;

    void connect(wl_signal *signal, std::function<void(void *)> callback)
    {
        Q_ASSERT(!isConnected());
        this->callback = std::move(callback);
        wl_signal_add(signal, &listener);
    }

    void disconnect()
    {
        if (isConnected()) {
            wl_list_remove(&listener.link);
            wl_list_init(&listener.link);
        }
        callback = {};
    }

    bool isConnected() const
    {
        return !wl_list_empty(&listener.link);
    }

private:
    static void handleEvent(wl_listener *listener, void *data)
    {
        WNativeListener *self;
        self = wl_container_of(listener, self, listener);
        const auto callback = self->callback;
        callback(data);
    }

    wl_listener listener;
    std::function<void(void *)> callback;
};

class WAYLIB_SERVER_EXPORT WWrapObjectPrivate : public WObjectPrivate
{
public:
    WWrapObjectPrivate(WWrapObject *q);
    ~WWrapObjectPrivate();

    template<typename Handle>
    inline Handle *handle() const {
        return qobject_cast<Handle*>(m_handle.get());
    }

protected:
    W_DECLARE_PUBLIC(WWrapObject)

    void initHandle(QW_NAMESPACE::qw_object_basic *handle);
    void invalidate();
    virtual void instantRelease() {}

    QList<QMetaObject::Connection> connectionsWithHandle;
    QPointer<QW_NAMESPACE::qw_object_basic> m_handle;
    uint invalidated:1;
};

#define WWRAP_HANDLE_FUNCTIONS(QW, WLR) \
inline QW *handle() const { \
    return WWrapObjectPrivate::handle<QW>(); \
} \
\
inline WLR *nativeHandle() const { \
    return handle()->handle(); \
}

WAYLIB_SERVER_END_NAMESPACE
