// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once
#include <wglobal.h>

#include <memory>

class QGuiApplication;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
WAYLIB_SERVER_END_NAMESPACE

namespace Treeland {
std::unique_ptr<QGuiApplication> preInit(int &argc, char *argv[]);
void postInit();
void initTestServer(WAYLIB_SERVER_NAMESPACE::WServer *server);
}
