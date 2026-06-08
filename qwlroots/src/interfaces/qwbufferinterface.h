// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwinterface.h>

extern "C" {
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
}

QW_BEGIN_NAMESPACE

QW_CLASS_INTERFACE(buffer)

QW_END_NAMESPACE
