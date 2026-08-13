// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "wwaylandresource.h"
#include "wglobal_p.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WWaylandResourcePrivate : public WObjectPrivate
{
public:
    using WObjectPrivate::WObjectPrivate;
    ~WWaylandResourcePrivate() override = default;

    virtual wl_client *waylandClient() const { return nullptr; }
};

WAYLIB_SERVER_END_NAMESPACE
