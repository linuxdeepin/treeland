// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#include <wlr/render/pass.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(render_pass)
{
public:
    QW_FUNC_MEMBER(render_pass, submit, bool)
    QW_FUNC_MEMBER(render_pass, add_texture, void, const wlr_render_texture_options *options)
    QW_FUNC_MEMBER(render_pass, add_rect, void, const wlr_render_rect_options *options)
};

QW_END_NAMESPACE
