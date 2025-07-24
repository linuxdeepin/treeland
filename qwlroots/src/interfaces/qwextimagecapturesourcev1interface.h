// Copyright (C) 2025 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwinterface.h>

extern "C" {
#include <wlr/interfaces/wlr_ext_image_capture_source_v1.h>
}

QW_BEGIN_NAMESPACE

using wlr_ext_image_capture_source_v1_impl = wlr_ext_image_capture_source_v1_interface;

QW_CLASS_INTERFACE(ext_image_capture_source_v1)

QW_END_NAMESPACE