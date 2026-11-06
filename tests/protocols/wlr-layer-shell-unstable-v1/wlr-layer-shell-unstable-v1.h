// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once
struct layer_shell_server_state { int wrapper; int container; int layer; int anchor; int exclusive_zone; int top_margin; int keyboard_exclusive; int focused; int on_primary; };
struct layer_shell_output_removal_state { int primary_before; int primary_after; int outputs_before; int outputs_after; };
struct layer_shell_window_state { int mapped; int owns_primary; int owns_output; int inside_own_output; };
