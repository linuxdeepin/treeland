// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#include <wlr/render/dmabuf.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(dmabuf_attributes)
{
public:
    QW_FUNC_MEMBER(dmabuf_attributes, finish, void, void)

    QW_FUNC_STATIC(dmabuf_attributes, copy, bool, wlr_dmabuf_attributes *dst, const wlr_dmabuf_attributes *src)
};

QW_END_NAMESPACE
