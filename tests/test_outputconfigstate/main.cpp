// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "output/outputconfigstate.h"

#include <QObject>
#include <QTest>

class OutputConfigStateTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testMarkScreenAsPrimary()
    {
        OutputConfigState state;
        state.markScreenAsPrimary("HDMI-1");
        QVERIFY(state.wasScreenPrimary("HDMI-1"));
    }

    void testWasScreenPrimaryFalse()
    {
        OutputConfigState state;
        QVERIFY(!state.wasScreenPrimary("HDMI-1"));
    }

    void testClearOutputState()
    {
        OutputConfigState state;
        state.markScreenAsPrimary("HDMI-1");
        state.clearOutputState("HDMI-1");
        QVERIFY(!state.wasScreenPrimary("HDMI-1"));
    }

    void testMultipleOutputs()
    {
        OutputConfigState state;
        state.markScreenAsPrimary("HDMI-1");
        state.markScreenAsPrimary("DP-1");
        QVERIFY(state.wasScreenPrimary("HDMI-1"));
        QVERIFY(state.wasScreenPrimary("DP-1"));
        QVERIFY(!state.wasScreenPrimary("VGA-1"));
    }

    void testRecordCopyModeExit()
    {
        OutputConfigState state;
        state.recordCopyModeExit();
        QVERIFY(state.shouldRestoreCopyMode());
    }

    void testClearCopyModeIntent()
    {
        OutputConfigState state;
        state.recordCopyModeExit();
        state.clearCopyModeIntent();
        QVERIFY(!state.shouldRestoreCopyMode());
    }

    void testClearOutputStateDoesNotAffectCopyMode()
    {
        OutputConfigState state;
        state.recordCopyModeExit();
        state.clearOutputState("HDMI-1");
        QVERIFY(state.shouldRestoreCopyMode());
    }

    void testClearOutputStateNonExistent()
    {
        OutputConfigState state;
        state.markScreenAsPrimary("HDMI-1");
        state.clearOutputState("DP-1");
        QVERIFY(state.wasScreenPrimary("HDMI-1"));
    }

    void testMarkScreenAsPrimaryRepeated()
    {
        OutputConfigState state;
        state.markScreenAsPrimary("HDMI-1");
        state.markScreenAsPrimary("HDMI-1");
        QVERIFY(state.wasScreenPrimary("HDMI-1"));
    }
};

QTEST_MAIN(OutputConfigStateTest)
#include "main.moc"
