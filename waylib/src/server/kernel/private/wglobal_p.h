// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "wglobal.h"

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
    void invalidate(QObject *object = nullptr);
    bool isInvalidated() const { return invalidated; }

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
    virtual void instantRelease() {}

private:
    bool invalidated = false;

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

WAYLIB_SERVER_END_NAMESPACE
