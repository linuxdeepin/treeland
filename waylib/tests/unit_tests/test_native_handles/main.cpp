// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WServer>

#include <QTest>

#include <type_traits>
#include <utility>

struct wl_display;

WAYLIB_SERVER_USE_NAMESPACE

static_assert(std::is_same_v<decltype(std::declval<WServer &>().handle()), wl_display *>);

class NativeHandlesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void serverExposesNativeDisplay()
    {
        WServer server;
        QVERIFY(server.handle());
    }
};

QTEST_MAIN(NativeHandlesTest)
#include "main.moc"
