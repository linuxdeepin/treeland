// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwinterface.h>

extern "C" {
#include <wlr/backend.h>
#include <wlr/backend/multi.h>
#define static
#include <wlr/backend/drm.h>
#undef static
#include <wlr/backend/wayland.h>
#ifdef WLR_HAVE_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include <wlr/backend/libinput.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/interface.h>
}

QW_BEGIN_NAMESPACE

QW_CLASS_INTERFACE(backend)

QW_END_NAMESPACE
