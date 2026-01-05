// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Test: Reproduce SIGSEGV crash using VALID DConfig
// The config schema is at: /usr/share/dsg/configs/org.deepin.dde.treeland/org.deepin.dde.treeland.user.json
//
// The crash mechanism:
// 1. initializeInConfigThread connects to DConfig::valueChanged signal
//    QObject::connect(config, &DConfig::valueChanged, this, [this](key) { updateValue(key); })
// 2. updateValue calls: QMetaObject::invokeMethod(this, [this]() { updateProperty(...) })
// 3. Data object destroyed while lambda is queued
// 4. Lambda tries to access destroyed 'this' -> SIGSEGV

#include "treelanduserconfig.hpp"

#include <QObject>
#include <QTest>
#include <QThread>
#include <QEventLoop>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QStandardPaths>

#include <chrono>
#include <atomic>

class ValidConfigCrashTest : public QObject
{
    Q_OBJECT

private:
    // Static flag to track if DConfig is available
    static bool s_dConfigAvailable;
    static bool s_initialized;

private Q_SLOTS:

    void initTestCase()
    {
        qDebug() << "=== ValidConfigCrashTest ===";
        qDebug() << "Using VALID DConfig from /usr/share/dsg/configs/";
        qDebug() << "Config name: org.deepin.dde.treeland.user";

        // One-time check for DConfig availability
        // This check must be safe and not fail even if DConfig is unavailable
        if (!s_initialized) {
            TreelandUserConfig *testConfig = nullptr;
            try {
                testConfig = TreelandUserConfig::createByName("org.deepin.dde.treeland.user",
                                                              "org.deepin.dde.treeland",
                                                              "/dde");
            } catch (const std::exception &e) {
                qDebug() << "Exception creating DConfig:" << e.what();
                testConfig = nullptr;
            } catch (...) {
                qDebug() << "Unknown exception creating DConfig";
                testConfig = nullptr;
            }

            if (testConfig) {
                // Wait for initialization with simple loop (not QTRY_VERIFY to avoid assertion failures)
                // Increased timeout to 5 seconds (500 * 10ms) to allow DConfig initialization
                int waitCount = 0;
                while (!testConfig->isInitializeSucceed() && waitCount < 500) {
                    QThread::msleep(10);
                    QCoreApplication::processEvents();
                    waitCount++;
                }

                s_dConfigAvailable = testConfig->isInitializeSucceed();

                // Let initialization complete before deletion
                for (int i = 0; i < 100; ++i) {
                    QCoreApplication::processEvents();
                }

                delete testConfig;
            } else {
                s_dConfigAvailable = false;
            }
            s_initialized = true;
        }

        if (!s_dConfigAvailable) {
            qDebug() << "DConfig schema not available - all tests will skip";
        }
    }

    // Test 1: Simple creation and immediate deletion
    // If DConfig is valid, this should NOT crash
    // If it crashes, it means the signal connection lambda is unsafe
    void test_immediate_destroy_valid_dconfig()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 1: Immediate destroy with valid DConfig ===";

        for (int cycle = 0; cycle < 50; ++cycle) {
            // Use createByName with correct parameters like production code
            TreelandUserConfig *config = TreelandUserConfig::createByName("org.deepin.dde.treeland.user",
                                                                          "org.deepin.dde.treeland",
                                                                          "/dde");

            QVERIFY2(config != nullptr, qPrintable("createByName returned nullptr in cycle " + QString::number(cycle)));

            // Delete immediately
            delete config;

            // Process events
            QCoreApplication::processEvents();

            if (cycle % 10 == 0) {
                qDebug() << "  Cycle" << cycle << "- No crash yet";
            }
        }
    }

    // Test 2: Destroy after signal connection is established
    // The crash is most likely here because:
    // 1. DConfig finishes initialization
    // 2. initializeInConfigThread sets up valueChanged connection
    // 3. Main thread then deletes the config
    // 4. If we trigger property change, the lambda captures destroyed 'this'
    void test_destroy_after_initialization()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 2: Destroy after initialization ===";

        for (int cycle = 0; cycle < 30; ++cycle) {
            TreelandUserConfig *config = TreelandUserConfig::createByName("org.deepin.dde.treeland.user",
                                                                          "org.deepin.dde.treeland",
                                                                          "/dde");

            QVERIFY2(config != nullptr, qPrintable("createByName returned nullptr in cycle " + QString::number(cycle)));

            // Wait for initialization to complete
            int waitCount = 0;
            while (!config->isInitializeSucceed() && waitCount < 200) {
                QThread::msleep(10);
                QCoreApplication::processEvents();
                waitCount++;
            }

            if (!config->isInitializeSucceed()) {
                delete config;
                continue;  // Skip this cycle if init failed
            }

            auto dconfig = config->config();
            QVERIFY2(dconfig != nullptr, "config() returned nullptr");
            QVERIFY2(dconfig->isValid(), qPrintable("DConfig is not valid in cycle " + QString::number(cycle)));

            qDebug() << "  Cycle" << cycle << "- Config initialized and valid";

            // Process more events to allow initialization thread to complete
            // This is critical to avoid race conditions
            for (int i = 0; i < 200; ++i) {
                QCoreApplication::processEvents();
                QThread::usleep(10);
            }

            // Now delete - the valueChanged connection should be fully set up
            // If config emits signal before deletion finishes, crash happens
            delete config;

            // Process events - this is critical
            // If there are queued valueChanged signals, they execute now
            for (int i = 0; i < 50; ++i) {
                QCoreApplication::processEvents();
                QThread::msleep(2);
            }
        }
    }

    // Test 3: Trigger property change then destroy
    // This more directly triggers the crash
    void test_property_change_then_destroy()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 3: Property change then destroy ===";

        for (int cycle = 0; cycle < 20; ++cycle) {
            TreelandUserConfig *config = TreelandUserConfig::createByName("org.deepin.dde.treeland.user",
                                                                          "org.deepin.dde.treeland",
                                                                          "/dde");

            QVERIFY2(config != nullptr, qPrintable("createByName returned nullptr in cycle " + QString::number(cycle)));

            // Wait for initialization with timeout
            int waitCount = 0;
            while (!config->isInitializeSucceed() && waitCount < 200) {
                QThread::msleep(10);
                QCoreApplication::processEvents();
                waitCount++;
            }

            if (!config->isInitializeSucceed()) {
                delete config;
                continue;  // Skip this cycle if init failed
            }

            auto dconfig = config->config();
            QVERIFY2(dconfig != nullptr, "config() returned nullptr");
            QVERIFY2(dconfig->isValid(), qPrintable("DConfig is not valid in cycle " + QString::number(cycle)));

            qDebug() << "  Cycle" << cycle << "- DConfig is VALID";

            // Change a property to trigger valueChanged signal
            // This will cause updateValue to be called in config thread
            dconfig->setValue(QStringLiteral("themeName"),
                             QVariant::fromValue(QString("test_theme_" + QString::number(cycle))));

            // Immediately delete while valueChanged signal might be in flight
            delete config;
            config = nullptr;

            // Process events - the queued updateProperty lambdas execute
            // If Data was destroyed, this triggers SIGSEGV
            for (int i = 0; i < 50; ++i) {
                QCoreApplication::processEvents();
                QThread::msleep(2);
            }
        }
    }

    // Test 4: Rapid property changes then destroy
    // More aggressive test to trigger race condition
    void test_rapid_changes_then_destroy()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 4: Rapid property changes then destroy ===";

        for (int cycle = 0; cycle < 15; ++cycle) {
            TreelandUserConfig *config = TreelandUserConfig::createByName("org.deepin.dde.treeland.user",
                                                                          "org.deepin.dde.treeland",
                                                                          "/dde");

            QVERIFY2(config != nullptr, qPrintable("createByName returned nullptr in cycle " + QString::number(cycle)));

            // Wait for initialization with timeout
            int waitCount = 0;
            while (!config->isInitializeSucceed() && waitCount < 200) {
                QThread::msleep(10);
                QCoreApplication::processEvents();
                waitCount++;
            }

            if (!config->isInitializeSucceed()) {
                delete config;
                continue;  // Skip this cycle if init failed
            }

            auto dconfig = config->config();
            QVERIFY2(dconfig != nullptr, "config() returned nullptr");
            QVERIFY2(dconfig->isValid(), qPrintable("DConfig is not valid in cycle " + QString::number(cycle)));

            // Rapidly change multiple properties
            for (int j = 0; j < 10; ++j) {
                dconfig->setValue(QStringLiteral("themeName"),
                                 QVariant::fromValue(QString("theme_" + QString::number(j))));
                dconfig->setValue(QStringLiteral("activeColor"),
                                 QVariant::fromValue(QString("color_" + QString::number(j))));
                QThread::msleep(1);
            }

            // Delete while many updateValue calls are queued
            delete config;
            config = nullptr;

            // Process events to execute queued lambdas
            for (int i = 0; i < 100; ++i) {
                QCoreApplication::processEvents();
                QThread::msleep(1);
            }
        }
    }

    // Test 5: Multiple sequential operations - simulate multiple configs being created and destroyed
    void test_concurrent_configs()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 5: Multiple sequential config operations ===";

        // Run 3 sequential operations (sequential instead of concurrent)
        for (int index = 1; index <= 3; ++index) {
            for (int cycle = 0; cycle < 5; ++cycle) {
                TreelandUserConfig *config = TreelandUserConfig::createByName("org.deepin.dde.treeland.user",
                                                                              "org.deepin.dde.treeland",
                                                                              "/dde");

                QVERIFY2(config != nullptr, qPrintable("createByName returned nullptr at index " + QString::number(index) + " cycle " + QString::number(cycle)));

                // Wait for initialization with timeout
                int waitCount = 0;
                while (!config->isInitializeSucceed() && waitCount < 100) {
                    QThread::msleep(5);
                    QCoreApplication::processEvents();
                    waitCount++;
                }

                if (!config->isInitializeSucceed()) {
                    delete config;
                    continue;  // Skip this cycle if init failed
                }

                auto dconfig = config->config();
                QVERIFY2(dconfig != nullptr, "config() returned nullptr");
                QVERIFY2(dconfig->isValid(), qPrintable("DConfig is not valid at index " + QString::number(index)));

                dconfig->setValue(QStringLiteral("themeName"),
                                 QVariant::fromValue(QString("seq_" + QString::number(index))));

                delete config;

                for (int i = 0; i < 20; ++i) {
                    QCoreApplication::processEvents();
                    QThread::msleep(2);
                }
            }
        }

        // Process all remaining events
        for (int i = 0; i < 100; ++i) {
            QCoreApplication::processEvents();
            QThread::msleep(1);
        }
    }

    // Test 6: Stress test - high frequency creation/destruction
    void test_high_frequency_stress()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 6: High frequency stress test ===";

        // Read environment variable for configurable cycle count
        // Default: 20 for CI (fast), can be set to 100+ locally for comprehensive testing
        bool ok = false;
        int cycles = qEnvironmentVariableIntValue("TREELAND_STRESS_CYCLES", &ok);
        if (!ok || cycles <= 0) {
            cycles = 20;  // Default for CI environments
        }

        qDebug() << "  Running stress test with" << cycles << "cycles (set TREELAND_STRESS_CYCLES to override)";

        auto start = std::chrono::high_resolution_clock::now();

        for (int cycle = 0; cycle < cycles; ++cycle) {
            TreelandUserConfig *config = TreelandUserConfig::createByName("org.deepin.dde.treeland.user",
                                                                          "org.deepin.dde.treeland",
                                                                          "/dde");

            if (!config) {
                continue;  // Skip if creation failed
            }

            // Minimal wait - let initialization start
            QThread::msleep(1);
            QCoreApplication::processEvents();

            // Delete immediately
            delete config;

            // Process some events
            for (int i = 0; i < 5; ++i) {
                QCoreApplication::processEvents();
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        qDebug() << "  " << cycles << "cycles completed in" << duration.count() << "ms";
    }

    void cleanupTestCase()
    {
        // Final event processing to clean up any remaining objects
        for (int i = 0; i < 200; ++i) {
            QCoreApplication::processEvents();
        }

        qDebug() << "=== Test Complete ===";
    }
};

// Static member initialization
bool ValidConfigCrashTest::s_dConfigAvailable = false;
bool ValidConfigCrashTest::s_initialized = false;

#include "test_dconfig_valid_config.moc"

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    if (!qEnvironmentVariableIsSet("DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    // Create QGuiApplication (required for DConfig initialization)
    QGuiApplication app(argc, argv);
    ValidConfigCrashTest tc;
    return QTest::qExec(&tc, argc, argv);
}
