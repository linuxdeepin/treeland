// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/virtual-output/virtualoutputmanagerinterfacev1.h"
#include "server-bridge.h"

#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {

VirtualOutputInterfaceV1 *g_virtual_output = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    auto *manager = find_server_interface<VirtualOutputManagerInterfaceV1>(helper);
    Q_ASSERT(manager);
    QObject::connect(manager, &VirtualOutputManagerInterfaceV1::requestCreateVirtualOutput,
                     [](VirtualOutputInterfaceV1 *interface) { g_virtual_output = interface; });
}

extern "C" void virtual_output_emit_outputs(void *)
{
    if (g_virtual_output)
        g_virtual_output->sendOutputs(QStringLiteral("group1"), QByteArray("DP-1\0VGA-1", 10));
}

extern "C" void virtual_output_emit_error(void *)
{
    if (g_virtual_output)
        g_virtual_output->sendError(VirtualOutputInterfaceV1::INVALID_OUTPUT, QStringLiteral("test error"));
}
