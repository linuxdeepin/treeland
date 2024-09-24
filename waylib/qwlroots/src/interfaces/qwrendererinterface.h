// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwinterface.h>

extern "C" {
#define static
#include <wlr/render/interface.h>
#undef static
#include <wlr/render/wlr_renderer.h>
#include <libdrm/drm_fourcc.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/util/box.h>
}

QW_BEGIN_NAMESPACE

QW_CLASS_INTERFACE(renderer)

QW_END_NAMESPACE
