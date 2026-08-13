// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <sys/types.h> // pid_t

WAYLIB_SERVER_BEGIN_NAMESPACE

class WWaylandResourcePrivate;

// WWaylandResource extends WObject with wayland-client specific information
// (waylandClient/pid/pidFD). Only objects that represent (or wrap) a
// wl_client-owned resource should inherit this class; plain WObject
// subclasses (outputs, seats, servers, protocol managers, ...) keep
// inheriting WObject directly.
class WAYLIB_SERVER_EXPORT WWaylandResource : public WObject
{
public:
    [[nodiscard]] WClient *waylandClient() const;
    [[nodiscard]] virtual pid_t pid() const;
    [[nodiscard]] virtual int pidFD() const;

protected:
    // All wayland-resource subclasses have their own *Private; there is no
    // default construction. A deleted default constructor prevents
    // accidentally creating a WObjectPrivate where a
    // WWaylandResourcePrivate is required (which would make d_func()'s
    // reinterpret_cast produce undefined behaviour).
    WWaylandResource() = delete;

    WWaylandResource(WWaylandResourcePrivate &dd, WObject *parent = nullptr);
    ~WWaylandResource() override = default;

    W_DECLARE_PRIVATE(WWaylandResource)
};

WAYLIB_SERVER_END_NAMESPACE
