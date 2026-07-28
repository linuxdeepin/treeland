// Copyright (c) 2017, 2018 Drew DeVault
// Copyright (c) 2014 Jari Vetoniemi
// Copyright (c) 2023 The wlroots contributors
// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: MIT

/*
 * In wlroots 0.20.2, the ext_foreign_toplevel_image_capture_source structs
 * and functions are now declared in the upstream public header. This shim
 * header simply re-exports the upstream declarations so existing code that
 * includes it keeps working. TODO: remove this file once all call sites
 * include <wlr/types/wlr_ext_image_capture_source_v1.h> directly.
 */

#ifndef WLR_TYPES_WLR_EXT_FOREIGN_TOPLEVEL_IMAGE_CAPTURE_SOURCE_MANAGER_V1_H
#define WLR_TYPES_WLR_EXT_FOREIGN_TOPLEVEL_IMAGE_CAPTURE_SOURCE_MANAGER_V1_H

#include <wlr/types/wlr_ext_image_capture_source_v1.h>

#endif
