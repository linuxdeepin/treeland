// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-pointer-constraints-unstable-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include <wbackend.h>
#include <winputdevice.h>
#include <wseat.h>
#include <protocols/wpointerconstraintsv1.h>

#include <wlr/types/wlr_pointer_constraints_v1.h>

extern "C" {
#include <wlr/interfaces/wlr_pointer.h>
}

#include <cstdlib>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WPointerConstraintsV1 *g_mgr = nullptr;

// Captured constraint from newConstraint signal
wlr_pointer_constraint_v1 *g_constraint = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    // Attach a pointer device so wl_seat advertises pointer capability.
    auto *seat = helper->seat();
    if (seat) {
        auto *pointer = static_cast<struct wlr_pointer *>(calloc(1, sizeof(struct wlr_pointer)));
        wlr_pointer_init(pointer, nullptr, "test-pointer");
        auto *inputDevice = new WInputDevice(&pointer->base, true);
        seat->attachInputDevice(inputDevice);
    }

    g_mgr = find_server_interface<WPointerConstraintsV1>(helper);
    if (g_mgr) {
        QObject::connect(g_mgr, &WPointerConstraintsV1::newConstraint,
                         helper, [](wlr_pointer_constraint_v1 *constraint) {
                             g_constraint = constraint;
                         });
    }
}

void pointer_constraints_read_server_state(void *data)
{
    auto *state = static_cast<struct pointer_constraints_server_state *>(data);
    state->valid = 0;
    state->constraint_type = -1;

    if (!g_constraint)
        return;

    state->valid = 1;
    state->constraint_type = static_cast<int>(g_constraint->type);
}
