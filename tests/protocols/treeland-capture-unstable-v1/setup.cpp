// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/capture/capture.h"

#include <woutputrenderwindow.h>
#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

void protocol_test_setup(WServer *server)
{
    // The capture module does not need seat/output/compositor globals of its
    // own, but CaptureManagerV1 keeps a QPointer to a WOutputRenderWindow that
    // production code injects via setOutputRenderWindow() right after attach
    // (see src/seat/helper.cpp). Without it every new capture context is
    // created from an uninitialized pointer, so give the module a real render
    // window just like the production wiring does.
    auto *renderWindow = new WOutputRenderWindow();
    auto *capture = server->attach<CaptureManagerV1>();
    capture->setOutputRenderWindow(renderWindow);
}
