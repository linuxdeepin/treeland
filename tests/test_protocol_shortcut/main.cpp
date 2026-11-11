// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/shortcut/shortcutmanager.h"

#include <wserver.h>

#include <QKeyCombination>
#include <QObject>
#include <QTest>

class ShortcutTest : public QObject
{
    Q_OBJECT

    WAYLIB_SERVER_NAMESPACE::WServer *m_server = nullptr;

public:
    ShortcutTest(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:

    void initTestCase()
    {
        m_server = new WAYLIB_SERVER_NAMESPACE::WServer();
    }

    void testCreate()
    {
        QVERIFY(m_server->attach<ShortcutManagerV3>(m_server) != nullptr);
    }

    void verifyShortcut()
    {
        auto protocol = m_server->findChild<ShortcutManagerV3*>();
        QVERIFY(protocol != nullptr);
    }

    // Shift+Tab's keysym is ISO_Left_Tab; Qt's QXkbCommon::keysymToQtKey() turns
    // it into Key_Backtab, while bindings spell the combination "Shift+Tab".
    // Both sides must normalize to one identity or the shortcut never matches.
    void testBacktabNormalization_data()
    {
        QTest::addColumn<QString>("bindKey");
        QTest::addColumn<Qt::KeyboardModifiers>("modifiers");

        QTest::newRow("Shift+Tab")
            << QStringLiteral("Shift+Tab") << Qt::KeyboardModifiers(Qt::ShiftModifier);
        QTest::newRow("Ctrl+Shift+Tab")
            << QStringLiteral("Ctrl+Shift+Tab")
            << Qt::KeyboardModifiers(Qt::ControlModifier | Qt::ShiftModifier);
        QTest::newRow("Alt+Shift+Tab")
            << QStringLiteral("Alt+Shift+Tab")
            << Qt::KeyboardModifiers(Qt::AltModifier | Qt::ShiftModifier);
    }

    void testBacktabNormalization()
    {
        QFETCH(QString, bindKey);
        QFETCH(Qt::KeyboardModifiers, modifiers);

        const int bindSide = ShortcutController::normalizedKeyCombinedFor(bindKey);
        const int eventSide =
            ShortcutController::normalizeKeyCombination(QKeyCombination(modifiers, Qt::Key_Backtab))
                .toCombined();

        QVERIFY(bindSide != 0);
        QCOMPARE(eventSide, bindSide);
        QCOMPARE(QKeyCombination::fromCombined(bindSide).key(), Qt::Key_Tab);
    }

    void cleanupTestCase()
    {
        m_server->deleteLater();
        m_server = nullptr;
    }
};

QTEST_MAIN(ShortcutTest)
#include "main.moc"
