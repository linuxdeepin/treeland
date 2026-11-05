// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

struct data_device_focus_request {
    unsigned index;
    int clear_first;
    int focused;
};

#ifdef __cplusplus
extern "C" {
#endif
void data_device_focus(void *data);
#ifdef __cplusplus
}
#endif
