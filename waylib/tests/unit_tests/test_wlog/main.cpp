// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wlogging.h>

#include <QtTest>
#include <cstdarg>

WAYLIB_SERVER_USE_NAMESPACE

// WLog routes wlroots' C logging (wlr_log) through a configurable callback;
// the default callback forwards to the "wlroots" Qt logging category. These
// tests verify the routing mechanism with a custom callback (the default
// callback's Qt output is environment-dependent and covered by manual runs).

static int g_messageCount = 0;
static wlr_log_importance g_lastVerbosity = WLR_SILENT;

static void testCallback(wlr_log_importance verbosity, const char *fmt, va_list args)
{
    Q_UNUSED(fmt);
    Q_UNUSED(args);
    g_messageCount++;
    g_lastVerbosity = verbosity;
}

class TestWLog : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initInstallsCallback();
    void initVerbosityFilters();
    void initRestoresDefaultCallback();
};

void TestWLog::initInstallsCallback()
{
    g_messageCount = 0;
    WLog::init(WLR_DEBUG, &testCallback);

    wlr_log(WLR_INFO, "test %d", 42);
    wlr_log(WLR_ERROR, "boom");
    QCOMPARE(g_messageCount, 2);
    QCOMPARE(g_lastVerbosity, WLR_ERROR);
}

void TestWLog::initVerbosityFilters()
{
    g_messageCount = 0;
    WLog::init(WLR_ERROR, &testCallback);

    // wlroots records the requested verbosity...
    QCOMPARE(wlr_log_get_verbosity(), WLR_ERROR);
    // ...but filtering is the callback's responsibility: a custom callback
    // receives every message (the default stderr/Qt callbacks filter).
    wlr_log(WLR_INFO, "custom callbacks see everything");
    wlr_log(WLR_ERROR, "and errors too");
    QCOMPARE(g_messageCount, 2);
    QCOMPARE(g_lastVerbosity, WLR_ERROR);
}

void TestWLog::initRestoresDefaultCallback()
{
    g_messageCount = 0;
    // The default callback must be installable through the same entry point
    // and must not crash when wlroots emits messages.
    WLog::init();
    wlr_log(WLR_INFO, "through default callback %s", "ok");
    wlr_log(WLR_DEBUG, "debug through default callback");
    WLog::init(WLR_DEBUG, &testCallback);
    wlr_log(WLR_INFO, "back to custom");
    QCOMPARE(g_messageCount, 1);
}

QTEST_GUILESS_MAIN(TestWLog)

#include "main.moc"
