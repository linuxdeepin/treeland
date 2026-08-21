// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*server_thread_callback)(void *data);

// Runs CALLBACK on the compositor's Qt thread and waits for it to finish.
int invoke_on_server_thread(server_thread_callback callback, void *data);

#ifdef __cplusplus
}
#endif
