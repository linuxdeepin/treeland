// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include <wglobal.h>
#include <functional>
#include <QString>
class QPlatformTheme;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
WAYLIB_SERVER_END_NAMESPACE

namespace Treeland {
struct InitOptions {
    bool headless = false;
    std::function<QPlatformTheme *(const QString &)> createPlatformTheme;
};
void preInit(const InitOptions &opts = {});
void postInit();
void initTestServer(WAYLIB_SERVER_NAMESPACE::WServer *server);
}
