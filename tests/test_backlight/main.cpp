// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "output/backlight.h"

class TestBacklight : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    bool createBacklightFiles(const QString &name, int maxBrightness, int currentBrightness)
    {
        QDir dir(m_tempDir.path());
        QDir subDir(dir.filePath(name));
        if (!subDir.exists()) {
            if (!dir.mkdir(name))
                return false;
        }

        QFile maxFile(subDir.filePath("max_brightness"));
        if (!maxFile.open(QIODevice::WriteOnly))
            return false;
        maxFile.write(QByteArray::number(maxBrightness));
        maxFile.close();

        QFile brFile(subDir.filePath("brightness"));
        if (!brFile.open(QIODevice::WriteOnly))
            return false;
        brFile.write(QByteArray::number(currentBrightness));
        brFile.close();

        return true;
    }

private Q_SLOTS:
    void initTestCase();
    void testBrightness();
    void testSetBrightness();
    void testSetBrightnessFull();
    void testSetBrightnessZero();
    void testSetBrightnessSameLevel();
    void testSetBrightnessClamp();
};

void TestBacklight::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
    QVERIFY(createBacklightFiles("intel_backlight", 100, 50));
}

void TestBacklight::testBrightness()
{
    Backlight bl("intel_backlight", m_tempDir.path());
    QCOMPARE(bl.brightness(), 0.5);
}

void TestBacklight::testSetBrightness()
{
    createBacklightFiles("test_set", 100, 50);
    Backlight bl("test_set", m_tempDir.path());
    qreal result = bl.setBrightness(0.8);
    QVERIFY(qFuzzyCompare(result, 0.8));
}

void TestBacklight::testSetBrightnessFull()
{
    createBacklightFiles("test_full", 100, 50);
    Backlight bl("test_full", m_tempDir.path());
    qreal result = bl.setBrightness(1.0);
    QCOMPARE(result, 1.0);
}

void TestBacklight::testSetBrightnessZero()
{
    createBacklightFiles("test_zero", 100, 50);
    Backlight bl("test_zero", m_tempDir.path());
    qreal result = bl.setBrightness(0.0);
    QCOMPARE(result, 0.0);
}

void TestBacklight::testSetBrightnessSameLevel()
{
    createBacklightFiles("test_same", 100, 50);
    Backlight bl("test_same", m_tempDir.path());
    qreal result = bl.setBrightness(0.5);
    QCOMPARE(result, 0.5);
}

void TestBacklight::testSetBrightnessClamp()
{
    createBacklightFiles("test_clamp", 100, 50);
    Backlight bl("test_clamp", m_tempDir.path());
    qreal result = bl.setBrightness(1.5);
    QCOMPARE(result, 1.0);
}

QTEST_MAIN(TestBacklight)
#include "main.moc"
