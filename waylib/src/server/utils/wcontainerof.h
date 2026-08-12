// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <cstddef>
#include <type_traits>

// Obtain the containing object of a member (offset-based, wlroots
// wl_container_of style). `offset` is normally produced by W_CONTAINER_OF
// with offsetof(Type, member); a raw offset also allows computing a pointer
// into a non-standard-layout prefix as long as T itself is standard-layout.
template<typename T, typename M>
static inline T *container_of(M *ptr, std::size_t offset) noexcept
{
    static_assert(std::is_standard_layout_v<T>,
                  "T must be standard-layout");

    return reinterpret_cast<T *>(
        reinterpret_cast<char *>(ptr) - offset);
}

// Obtain the containing object of a member; use this instead of wlroots'
// unguarded wl_container_of macro. The static_assert in container_of() is
// unconditional: non-standard-layout containers must not be recovered this
// way even when the member sits at offset 0 (use a registry or the native
// object's data field instead).
#define W_CONTAINER_OF(ptr, Type, member) \
    container_of<Type>(ptr, offsetof(Type, member))
