// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/shortcut/shortcutcontroller.h"
#include "modules/shortcut/shortcutmanager.h"

#include <QObject>
#include <QTest>
#include <QSignalSpy>
#include <QKeyEvent>

Q_DECLARE_METATYPE(ShortcutAction)

class ShortcutControllerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testNormalizeCtrlKey()
    {
        auto result = ShortcutController::normalizeKeyCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_Control));
        QCOMPARE(result.keyboardModifiers(), Qt::ControlModifier);
        QCOMPARE(result.key(), Qt::Key_unknown);
    }

    void testNormalizeShiftKey()
    {
        auto result = ShortcutController::normalizeKeyCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_Shift));
        QCOMPARE(result.keyboardModifiers(), Qt::ShiftModifier);
        QCOMPARE(result.key(), Qt::Key_unknown);
    }

    void testNormalizeAltKey()
    {
        auto result = ShortcutController::normalizeKeyCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_Alt));
        QCOMPARE(result.keyboardModifiers(), Qt::AltModifier);
        QCOMPARE(result.key(), Qt::Key_unknown);
    }

    void testNormalizeMetaKey()
    {
        auto result = ShortcutController::normalizeKeyCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_Meta));
        QCOMPARE(result.keyboardModifiers(), Qt::MetaModifier);
        QCOMPARE(result.key(), Qt::Key_unknown);
    }

    void testNormalizeSuperLKey()
    {
        auto result = ShortcutController::normalizeKeyCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_Super_L));
        QCOMPARE(result.keyboardModifiers(), Qt::MetaModifier);
        QCOMPARE(result.key(), Qt::Key_unknown);
    }

    void testNormalizeSuperRKey()
    {
        auto result = ShortcutController::normalizeKeyCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_Super_R));
        QCOMPARE(result.keyboardModifiers(), Qt::MetaModifier);
        QCOMPARE(result.key(), Qt::Key_unknown);
    }

    void testNormalizeCtrlCombo()
    {
        auto result = ShortcutController::normalizeKeyCombination(
            QKeyCombination(Qt::ControlModifier, Qt::Key_A));
        QCOMPARE(result.keyboardModifiers(), Qt::ControlModifier);
        QCOMPARE(result.key(), Qt::Key_A);
    }

    void testNormalizePlainKey()
    {
        auto result = ShortcutController::normalizeKeyCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_A));
        QCOMPARE(result.keyboardModifiers(), Qt::NoModifier);
        QCOMPARE(result.key(), Qt::Key_A);
    }

    void testIsValidMetaOnly()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::MetaModifier, Qt::Key_unknown)));
    }

    void testIsInvalidCtrlOnly()
    {
        QVERIFY(!ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ControlModifier, Qt::Key_unknown)));
    }

    void testIsInvalidAltOnly()
    {
        QVERIFY(!ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::AltModifier, Qt::Key_unknown)));
    }

    void testIsInvalidShiftOnly()
    {
        QVERIFY(!ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ShiftModifier, Qt::Key_unknown)));
    }

    void testIsValidCtrlA()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ControlModifier, Qt::Key_A)));
    }

    void testIsValidAltF4()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::AltModifier, Qt::Key_F4)));
    }

    void testIsValidMetaSpace()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::MetaModifier, Qt::Key_Space)));
    }

    void testIsValidF1()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_F1)));
    }

    void testIsValidF35()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_F35)));
    }

    void testIsValidDelete()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_Delete)));
    }

    void testIsValidPrint()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_Print)));
    }

    void testIsValidShiftSpace()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ShiftModifier, Qt::Key_Space)));
    }

    void testIsValidShiftEscape()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ShiftModifier, Qt::Key_Escape)));
    }

    void testIsValidShiftTab()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ShiftModifier, Qt::Key_Tab)));
    }

    void testIsInvalidShiftA()
    {
        QVERIFY(!ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ShiftModifier, Qt::Key_A)));
    }

    void testIsInvalidPlainA()
    {
        QVERIFY(!ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_A)));
    }

    void testIsValidMediaKey()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_MediaPlay)));
    }

    void testIsValidBackspaceNoModifier()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::NoModifier, Qt::Key_Backspace)));
    }

    void testIsValidShiftHome()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ShiftModifier, Qt::Key_Home)));
    }

    void testIsValidShiftLeft()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ShiftModifier, Qt::Key_Left)));
    }

    void testIsValidShiftUp()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ShiftModifier, Qt::Key_Up)));
    }

    void testIsValidShiftDown()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ShiftModifier, Qt::Key_Down)));
    }

    void testIsValidShiftRight()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ShiftModifier, Qt::Key_Right)));
    }

    void testIsValidShiftPageUp()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ShiftModifier, Qt::Key_PageUp)));
    }

    void testIsValidShiftPageDown()
    {
        QVERIFY(ShortcutController::isValidShortcutCombination(
            QKeyCombination(Qt::ShiftModifier, Qt::Key_PageDown)));
    }

    void testRegisterKeySuccess()
    {
        ShortcutController ctrl;
        uint result = ctrl.registerKey("test", "Ctrl+A", ShortcutController::KeyPress, ShortcutAction::Notify);
        QCOMPARE(result, 0u);
    }

    void testRegisterKeyNameConflict()
    {
        ShortcutController ctrl;
        ctrl.registerKey("test", "Ctrl+A", ShortcutController::KeyPress, ShortcutAction::Notify);
        uint result = ctrl.registerKey("test", "Ctrl+B", ShortcutController::KeyPress, ShortcutAction::Workspace1);
        QVERIFY(result != 0);
    }

    void testRegisterKeyInvalidKey()
    {
        ShortcutController ctrl;
        uint result = ctrl.registerKey("bad", "A", ShortcutController::KeyPress, ShortcutAction::Notify);
        QVERIFY(result != 0);
    }

    void testRegisterKeySameKeyDifferentName()
    {
        ShortcutController ctrl;
        uint result1 = ctrl.registerKey("name1", "Ctrl+A", ShortcutController::KeyPress, ShortcutAction::Notify);
        QCOMPARE(result1, 0u);
        uint result2 = ctrl.registerKey("name2", "Ctrl+A", ShortcutController::KeyPress, ShortcutAction::Workspace1);
        QCOMPARE(result2, 0u);
    }

    void testUnregisterShortcut()
    {
        ShortcutController ctrl;
        ctrl.registerKey("test", "Ctrl+A", ShortcutController::KeyPress, ShortcutAction::Notify);
        ctrl.unregisterShortcut("test");
        QSignalSpy spy(&ctrl, &ShortcutController::actionTriggered);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
        ctrl.dispatchKeyEvent(&event);
        QCOMPARE(spy.count(), 0);
    }

    void testDispatchKeyEventMatch()
    {
        ShortcutController ctrl;
        ctrl.registerKey("test", "Ctrl+A", ShortcutController::KeyPress, ShortcutAction::Notify);
        QSignalSpy spy(&ctrl, &ShortcutController::actionTriggered);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
        ctrl.dispatchKeyEvent(&event);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<ShortcutAction>(), ShortcutAction::Notify);
        QCOMPARE(spy.at(0).at(1).toString(), QString("test"));
        QCOMPARE(spy.at(0).at(2).toBool(), false);
    }

    void testDispatchKeyEventNoMatch()
    {
        ShortcutController ctrl;
        ctrl.registerKey("test", "Ctrl+A", ShortcutController::KeyPress, ShortcutAction::Notify);
        QSignalSpy spy(&ctrl, &ShortcutController::actionTriggered);
        QKeyEvent event(QEvent::KeyPress, Qt::Key_B, Qt::ControlModifier);
        ctrl.dispatchKeyEvent(&event);
        QCOMPARE(spy.count(), 0);
    }

    void testDispatchKeyEventFlagsFilter()
    {
        ShortcutController ctrl;
        ctrl.registerKey("test", "Ctrl+A", ShortcutController::KeyPress, ShortcutAction::Notify);
        QSignalSpy spy(&ctrl, &ShortcutController::actionTriggered);
        QKeyEvent event(QEvent::KeyRelease, Qt::Key_A, Qt::ControlModifier);
        ctrl.dispatchKeyEvent(&event);
        QCOMPARE(spy.count(), 0);
    }

    void testModifierForAction()
    {
        ShortcutController ctrl;
        ctrl.registerKey("test", "Ctrl+A", ShortcutController::KeyPress, ShortcutAction::Notify);
        QCOMPARE(ctrl.modifierForAction(ShortcutAction::Notify), Qt::ControlModifier);
    }

    void testModifierForActionNotFound()
    {
        ShortcutController ctrl;
        QCOMPARE(ctrl.modifierForAction(ShortcutAction::Notify), Qt::NoModifier);
    }
};

QTEST_MAIN(ShortcutControllerTest)
#include "main.moc"
