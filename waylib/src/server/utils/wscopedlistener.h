// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wayland-server-core.h>
#include <cstddef>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WScopedListener
{
public:
    WScopedListener() = default;
    ~WScopedListener() { remove(); }

    WScopedListener(const WScopedListener &) = delete;
    WScopedListener &operator=(const WScopedListener &) = delete;

    void connect(wl_signal *signal, wl_listener_notify_func_t notify)
    {
        remove();
        m_listener.notify = notify;
        wl_signal_add(signal, &m_listener);
        m_connected = true;
    }

    void remove()
    {
        if (m_connected) {
            wl_list_remove(&m_listener.link);
            m_connected = false;
        }
    }

    bool isConnected() const { return m_connected; }

    wl_listener *listener() { return &m_listener; }
    const wl_listener *listener() const { return &m_listener; }

    // Recover the owning object T from a wl_listener that is the m_listener of
    // a WScopedListener member. Member is a pointer to that WScopedListener
    // member of T. This works for non-standard-layout T (e.g. classes with a
    // base), and is -Wpedantic clean.
    template<typename T, WScopedListener T::*Member>
    static T *owner(wl_listener *l)
    {
        auto *wsl = reinterpret_cast<WScopedListener *>(
            reinterpret_cast<char *>(l) - offsetof(WScopedListener, m_listener));
        auto off = reinterpret_cast<ptrdiff_t>(&(static_cast<T *>(nullptr)->*Member));
        return reinterpret_cast<T *>(reinterpret_cast<char *>(wsl) - off);
    }

private:
    wl_listener m_listener;
    bool m_connected = false;
};

WAYLIB_SERVER_END_NAMESPACE
