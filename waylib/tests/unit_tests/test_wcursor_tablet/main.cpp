// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <WCursor>
#include <WSeat>
#include <WServer>

#include <wlr_all.h>

#include <QtTest>

WAYLIB_SERVER_USE_NAMESPACE

// This test verifies that WCursorPrivate::connect() registers listeners for
// tablet_tool_* events on the underlying wlr_cursor. Before the fix, connect()
// only registered pointer and touch listeners; tablet events were silently
// dropped. After the fix, four new listeners (on_tablet_axis, on_tablet_button,
// on_tablet_tip, on_tablet_proximity) are added.
//
// The test creates a WServer, attaches a WSeat (which creates a wlr_seat via
// start()), creates a WCursor, and calls seat->setCursor(cursor). setCursor()
// calls cursor->setSeat(seat) (private, but WSeat is a friend of WCursor),
// which triggers WCursorPrivate::connect(). The test then checks that each
// tablet_tool_* signal on the wlr_cursor has at least one listener.

class TestWCursorTablet : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void tabletToolListenersRegistered();
};

void TestWCursorTablet::tabletToolListenersRegistered()
{
    WServer server;
    QVERIFY(server.handle());

    WSeat *seat = server.attach<WSeat>("test-seat");
    QVERIFY(seat);

    server.start();
    QVERIFY(seat->isValid());

    WCursor *cursor = new WCursor();
    seat->setCursor(cursor);

    wlr_cursor *handle = cursor->handle();
    QVERIFY(handle);

    QVERIFY2(!wl_list_empty(&handle->events.tablet_tool_axis.listener_list),
             "tablet_tool_axis listener not registered");
    QVERIFY2(!wl_list_empty(&handle->events.tablet_tool_button.listener_list),
             "tablet_tool_button listener not registered");
    QVERIFY2(!wl_list_empty(&handle->events.tablet_tool_tip.listener_list),
             "tablet_tool_tip listener not registered");
    QVERIFY2(!wl_list_empty(&handle->events.tablet_tool_proximity.listener_list),
             "tablet_tool_proximity listener not registered");

    seat->setCursor(nullptr);
    delete cursor;
    server.stop();
}

QTEST_GUILESS_MAIN(TestWCursorTablet)

#include "main.moc"
