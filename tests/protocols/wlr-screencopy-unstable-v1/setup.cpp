// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "seat/helper.h"

#include <woutputrenderwindow.h>

void protocol_test_setup(Helper *)
{
}

extern "C" void screencopy_render(void *)
{
    const auto outputs = Helper::instance()->rootSurfaceContainer()->outputs();
    for (auto *output : outputs)
        Helper::instance()->window()->render(output->screenViewport(), true);
}
