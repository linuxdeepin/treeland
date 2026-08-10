// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/virtual-output/virtualoutputmanagerinterfacev1.h"

#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
/* The virtual output object stored by the first successful create; the
 * client's "group1" proxy refers to this same object. */
VirtualOutputInterfaceV1 *g_virtual_output = nullptr;
}

void protocol_test_setup(WServer *server)
{
    auto *manager = server->attach<VirtualOutputManagerInterfaceV1>();
    QObject::connect(manager, &VirtualOutputManagerInterfaceV1::requestCreateVirtualOutput,
                     [](VirtualOutputInterfaceV1 *interface) { g_virtual_output = interface; });
}

/* Server-side stimulus: push an updated outputs event for the stored group. */
extern "C" void virtual_output_emit_outputs(void *)
{
    if (g_virtual_output)
        g_virtual_output->sendOutputs(QStringLiteral("group1"), QByteArray("DP-1\0VGA-1", 10));
}

/* Server-side stimulus: push an error event (invalid_output) on the stored group. */
extern "C" void virtual_output_emit_error(void *)
{
    if (g_virtual_output)
        g_virtual_output->sendError(VirtualOutputInterfaceV1::INVALID_OUTPUT, QStringLiteral("test error"));
}
