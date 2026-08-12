// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wscoplistener.h"
#include "wcontainerof.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WScopedListener::WScopedListener(WScopedListener &&other) noexcept
    : m_signal(other.m_signal)
    , m_callback(other.m_callback)
    , m_deleter(other.m_deleter)
    , m_context(other.m_context)
    , m_connected(std::exchange(other.m_connected, false))
{
    if (m_connected) {
        // The wl_listener node now lives at this instance's address, but the
        // signal still points at the old node: detach and re-link it.
        if (other.m_listener.link.next)
            wl_list_remove(&other.m_listener.link);
        m_listener.notify = &WScopedListener::trampoline;
        wl_signal_add(m_signal, &m_listener);
    } else {
        m_listener = other.m_listener;
    }
    other.m_signal = nullptr;
    other.m_callback = nullptr;
    other.m_deleter = nullptr;
    other.m_context = nullptr;
    other.m_listener = {};
}

WScopedListener &WScopedListener::operator=(WScopedListener &&other) noexcept
{
    if (this == &other)
        return *this;
    disconnect();
    if (m_deleter)
        m_deleter(m_context);
    m_signal = other.m_signal;
    m_callback = other.m_callback;
    m_deleter = other.m_deleter;
    m_context = other.m_context;
    m_connected = std::exchange(other.m_connected, false);
    if (m_connected) {
        if (other.m_listener.link.next)
            wl_list_remove(&other.m_listener.link);
        m_listener.notify = &WScopedListener::trampoline;
        wl_signal_add(m_signal, &m_listener);
    } else {
        m_listener = other.m_listener;
    }
    other.m_signal = nullptr;
    other.m_callback = nullptr;
    other.m_deleter = nullptr;
    other.m_context = nullptr;
    other.m_listener = {};
    return *this;
}

void WScopedListener::disconnect()
{
    if (!m_connected)
        return;
    if (m_listener.link.next)
        wl_list_remove(&m_listener.link);
    m_listener = {};
    m_connected = false;
    m_signal = nullptr;
    // The closure is kept in a shared_ptr (see initLambda): releasing it
    // here or in ~WScopedListener is safe even from inside the listener's
    // own callback — invoke() already holds its own copy, so the closure
    // lives until the emission returns.
}

void WScopedListener::trampoline(wl_listener *listener, void *data)
{
    // Capture the closure (and thus a reference to it) before invoking:
    // the callback may destroy this listener, which releases the closure
    // storage; the shared_ptr copy keeps the closure alive until the
    // emission returns.
    WScopedListener *self = nullptr;
    self = W_CONTAINER_OF(listener, WScopedListener, m_listener);
    self->m_callback(self->m_context, data);
}

WAYLIB_SERVER_END_NAMESPACE
