// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_linux_dmabuf_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_BOX(linux_dmabuf_feedback_v1)
{
public:
    qw_linux_dmabuf_feedback_v1() = delete;
    qw_linux_dmabuf_feedback_v1(const wlr_linux_dmabuf_feedback_v1_init_options *options) {
        init_with_options(options);
    }

private:
    friend class qw_class_box;
    QW_FUNC_MEMBER(linux_dmabuf_feedback_v1, finish, void)
    QW_FUNC_MEMBER(linux_dmabuf_feedback_v1, init_with_options, bool, const wlr_linux_dmabuf_feedback_v1_init_options *options)
};

class QW_CLASS_OBJECT(linux_dmabuf_v1)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(linux_dmabuf_v1, create, qw_linux_dmabuf_v1 *, wl_display *display, uint32_t version, const wlr_linux_dmabuf_feedback_v1 *default_feedback)
};

QW_END_NAMESPACE
