// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

#include <QAbstractEventDispatcher>
#include <QSocketNotifier>
#include <QThread>

#include <wayland-server.h>

struct wl_event_loop;
QW_BEGIN_NAMESPACE

class QW_EXPORT qw_display : public qw_object<wl_display, qw_display>
{
    Q_OBJECT
    using qw_object::qw_object;
    friend class qw_object;

#define QW_DISPLAY_FUNC_MEMBER(wl_func_suffix, ret_type, ...) \
    template<typename ...Args> \
    QW_ALWAYS_INLINE ret_type \
    wl_func_suffix(Args &&... args) const requires std::is_invocable_v<void(*)(__VA_ARGS__), Args...> \
    { \
        static_assert(std::is_invocable_v<decltype(wl_display##_##wl_func_suffix), decltype(*this), Args...>, ""); \
        return wl_display##_##wl_func_suffix(*this, std::forward<Args>(args)...); \
    }

public:
    explicit qw_display() : qw_object(wl_display_create(), true) { }

    QW_DISPLAY_FUNC_MEMBER(get_event_loop, wl_event_loop*)
    QW_DISPLAY_FUNC_MEMBER(add_socket, int, const char *name)
    QW_DISPLAY_FUNC_MEMBER(add_socket_fd, int, int sock_fd)
    QW_DISPLAY_FUNC_MEMBER(add_socket_auto, const char*)
    QW_DISPLAY_FUNC_MEMBER(run, void)
    QW_DISPLAY_FUNC_MEMBER(terminate, void)
    QW_DISPLAY_FUNC_MEMBER(flush_clients, void)
    QW_DISPLAY_FUNC_MEMBER(destroy_clients, void)
    QW_DISPLAY_FUNC_MEMBER(get_serial, uint32_t)
    QW_DISPLAY_FUNC_MEMBER(next_serial, uint32_t)
    QW_DISPLAY_FUNC_MEMBER(set_global_filter, void, wl_display_global_filter_func_t, void*)
    QW_DISPLAY_FUNC_MEMBER(init_shm, int)
    QW_DISPLAY_FUNC_MEMBER(add_shm_format, uint32_t*, uint32_t)
    QW_DISPLAY_FUNC_MEMBER(add_protocol_logger, wl_protocol_logger*, wl_protocol_logger_func_t, void*)

    void start(QThread *thread) {
        auto loop = this->get_event_loop();
        int fd = wl_event_loop_get_fd(loop);

        auto processWaylandEvents = [this, loop] () {
            int ret = wl_event_loop_dispatch(loop, 0);
            if (ret)
                fprintf(stderr, "wl_event_loop_dispatch error: %d\n", ret);
            this->flush_clients();
        };

        auto sockNot = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(sockNot, &QSocketNotifier::activated, this, processWaylandEvents);

        QAbstractEventDispatcher *dispatcher = thread->eventDispatcher();
        connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this, processWaylandEvents);
    }

protected:
    QW_DISPLAY_FUNC_MEMBER(destroy, void)
};

QW_END_NAMESPACE
