// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

struct screencopy_render_state {
    int output_count;
    int output_enabled_before;
    int needs_frame_before;
    int frame_pending_before;
    int attach_render_locks_before;
    int render_end_count;
    int target_committed;
    int output_enabled_after;
    int needs_frame_after;
    int frame_pending_after;
    int attach_render_locks_after;
};
