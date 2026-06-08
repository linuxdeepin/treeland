// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwinterface.h>

extern "C" {
#define static
#include <wlr/types/wlr_output.h>
#undef static
#include <wlr/interfaces/wlr_output.h>
}

QW_BEGIN_NAMESPACE

QW_CLASS_INTERFACE(output)

QW_END_NAMESPACE
