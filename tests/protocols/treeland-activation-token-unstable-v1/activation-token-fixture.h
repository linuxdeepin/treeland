// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Readback of the production ActivationManagerInterfaceV1::activateRequested
// signal, used to prove a minted token resolves to a real activation.
// TokenDisposition: 0=Invalid, 1=Attention, 2=Active
struct activation_token_server_state {
    int activate_requested;
    int disposition;
};

void activation_token_read_server_state(void *data);

#ifdef __cplusplus
}
#endif
