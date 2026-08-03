// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_buffer.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(buffer)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(release)

public:
    struct droper {
        static inline void cleanup(qw_buffer *pointer) { if (pointer) pointer->drop(); }
        void operator()(qw_buffer *pointer) const { cleanup(pointer); }
    };

    struct unlocker {
        static inline void cleanup(qw_buffer *pointer) { if (pointer) pointer->unlock(); }
        void operator()(qw_buffer *pointer) const { cleanup(pointer); }
    };

    struct releaser {
        static inline void cleanup(qw_buffer *pointer) {
            if (pointer) {
                pointer->unlock();
                pointer->drop();
            }
        }
        void operator()(qw_buffer *pointer) const { cleanup(pointer); }
    };

    QW_FUNC_STATIC(buffer, try_from_resource, qw_buffer *, wl_resource *resource)

    QW_FUNC_MEMBER(buffer, drop, void)
    QW_FUNC_MEMBER(buffer, lock, wlr_buffer *)
    QW_FUNC_MEMBER(buffer, unlock, void)
    QW_FUNC_MEMBER(buffer, get_dmabuf, bool, wlr_dmabuf_attributes *attribs)
    QW_FUNC_MEMBER(buffer, get_shm, bool, wlr_shm_attributes *attribs)
    QW_FUNC_MEMBER(buffer, begin_data_ptr_access, bool, uint32_t flags, void **data, uint32_t *format, size_t *stride)
    QW_FUNC_MEMBER(buffer, end_data_ptr_access, void)
};

class QW_CLASS_REINTERPRET_CAST(client_buffer)
{
public:
    QW_FUNC_STATIC(client_buffer, get, qw_client_buffer *, wlr_buffer *buffer)
};

QW_END_NAMESPACE
