// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Test: Reproduce SIGSEGV crash using VALID DConfig
// The config schema is at:
// /usr/share/dsg/configs/org.deepin.dde.treeland/org.deepin.dde.treeland.user.json
//
// Historical crash mechanism (pre-fix):
// 1. initializeInConfigThread connects to DConfig::valueChanged signal
//    QObject::connect(config, &DConfig::valueChanged, this, [this](key) { updateValue(key); })
// 2. updateValue calls: QMetaObject::invokeMethod(this, [this]() { updateProperty(...) })
// 3. Data object destroyed while lambda is queued
// 4. Lambda tries to access destroyed 'this' -> SIGSEGV

#include "treelanduserconfig.hpp"

#include <QGuiApplication>
#include <QObject>
#include <QTest>
#include <QThread>

#include <atomic>
#include <chrono>

class ValidConfigCrashTest : public QObject
{
    Q_OBJECT

private:
    // Constants for test configuration
    static constexpr int IMMEDIATE_DESTROY_CYCLES = 50;
    static constexpr int POST_INIT_DESTROY_CYCLES = 30;
    static constexpr int MID_INIT_DESTROY_CYCLES = 100;
    static constexpr int PROPERTY_CHANGE_CYCLES = 20;
    static constexpr int RAPID_CHANGE_CYCLES = 15;
    static constexpr int CONCURRENT_CONFIG_CYCLES = 5;

    static constexpr int DEFAULT_STRESS_CYCLES = 20;

    static constexpr int INIT_TIMEOUT_ITERATIONS = 500;
    static constexpr int INIT_TIMEOUT_MS = 10;

    static constexpr int CLEANUP_CYCLES = 50;
    static constexpr int CLEANUP_SLEEP_MS = 2;

    // Static flag to track if DConfig is available
    static bool s_dConfigAvailable;
    static bool s_initialized;

    // Helper for standardized cleanup
    void waitForCleanup(int cycles = CLEANUP_CYCLES, int sleepMs = CLEANUP_SLEEP_MS) const
    {
        for (int i = 0; i < cycles; ++i) {
            QCoreApplication::processEvents();
            if (sleepMs > 0) {
                QThread::msleep(sleepMs);
            }
        }
    }

    // Helper to create config
    TreelandUserConfig *createConfig() const
    {
        return TreelandUserConfig::createByName(QStringLiteral("org.deepin.dde.treeland.user"),
                                                QStringLiteral("org.deepin.dde.treeland"),
                                                QStringLiteral("/dde"));
    }

private Q_SLOTS:

    void initTestCase()
    {
        qDebug() << "=== ValidConfigCrashTest ===";
        qDebug() << "Using VALID DConfig from /usr/share/dsg/configs/";
        qDebug() << "Config name: org.deepin.dde.treeland.user";

        // One-time check for DConfig availability
        if (!s_initialized) {
            TreelandUserConfig *testConfig = createConfig();

            if (testConfig) {
                // Wait for initialization
                int waitCount = 0;
                while (!testConfig->isInitializeSucceeded() && waitCount < INIT_TIMEOUT_ITERATIONS) {
                    QThread::msleep(INIT_TIMEOUT_MS);
                    QCoreApplication::processEvents();
                    waitCount++;
                }

                s_dConfigAvailable = testConfig->isInitializeSucceeded();

                waitForCleanup(100, 0); // Cleanup after check
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
    void test_immediate_destroy_valid_dconfig()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 1: Immediate destroy with valid DConfig ===";

        for (int cycle = 0; cycle < IMMEDIATE_DESTROY_CYCLES; ++cycle) {
            TreelandUserConfig *config = createConfig();

            QVERIFY2(
                config != nullptr,
                qPrintable(QStringLiteral("createByName returned nullptr in cycle %1").arg(cycle)));

            // Delete immediately
            delete config;

            QCoreApplication::processEvents();

            if (cycle % 10 == 0) {
                qDebug() << "  Cycle" << cycle << "- No crash yet";
            }
        }
    }

    // Test 2: Destroy after signal connection is established
    void test_destroy_after_initialization()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 2: Destroy after initialization ===";

        for (int cycle = 0; cycle < POST_INIT_DESTROY_CYCLES; ++cycle) {
            TreelandUserConfig *config = createConfig();

            QVERIFY2(
                config != nullptr,
                qPrintable(QStringLiteral("createByName returned nullptr in cycle %1").arg(cycle)));

            // Wait for initialization
            int waitCount = 0;
            while (!config->isInitializeSucceeded() && waitCount < 200) {
                QThread::msleep(INIT_TIMEOUT_MS);
                QCoreApplication::processEvents();
                waitCount++;
            }

            if (!config->isInitializeSucceeded()) {
                delete config;
                continue;
            }

            auto dconfig = config->config();
            QVERIFY2(dconfig != nullptr, "config() returned nullptr");
            QVERIFY2(dconfig->isValid(),
                     qPrintable(QStringLiteral("DConfig is not valid in cycle %1").arg(cycle)));

            qDebug() << "  Cycle" << cycle << "- Config initialized and valid";

            // Process more events to allow initialization thread to complete
            for (int i = 0; i < 200; ++i) {
                QCoreApplication::processEvents();
                QThread::usleep(10);
            }

            delete config;
            waitForCleanup();
        }
    }

    // Test 2.5: Destroy DURING initialization (mid-initialization race condition)
    void test_destroy_during_initialization()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 2.5: Destroy DURING initialization (race condition) ===";

        for (int cycle = 0; cycle < MID_INIT_DESTROY_CYCLES; ++cycle) {
            TreelandUserConfig *config = createConfig();

            QVERIFY2(
                config != nullptr,
                qPrintable(QStringLiteral("createByName returned nullptr in cycle %1").arg(cycle)));

            // Delete after a VERY short delay to hit the initialization window
            // 0-900 microseconds
            int delayMicros = (cycle % 10) * 100;
            if (delayMicros > 0) {
                QThread::usleep(delayMicros);
            }

            delete config;
            QCoreApplication::processEvents();

            if (cycle % 20 == 0) {
                qDebug() << "  Cycle" << cycle << "- Testing with" << delayMicros << "μs delay";
            }
        }

        qDebug() << "  Mid-initialization destruction test completed - no crashes";
    }

    // Test 2.6: Verify signal thread affinity
    void test_signal_thread_affinity()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 2.6: Signal thread affinity verification ===";

        QThread *mainThread = QThread::currentThread();
        QThread *signalThread = nullptr;
        bool signalReceived = false;

        for (int cycle = 0; cycle < 5; ++cycle) {
            TreelandUserConfig *config = createConfig();

            QVERIFY2(
                config != nullptr,
                qPrintable(QStringLiteral("createByName returned nullptr in cycle %1").arg(cycle)));

            signalThread = nullptr;
            signalReceived = false;
            connect(config, &TreelandUserConfig::themeNameChanged, this, [&]() {
                signalThread = QThread::currentThread();
                signalReceived = true;
            });

            // Wait for initialization
            int waitCount = 0;
            while (!config->isInitializeSucceeded() && waitCount < 200) {
                QThread::msleep(INIT_TIMEOUT_MS);
                QCoreApplication::processEvents();
                waitCount++;
            }

            if (!config->isInitializeSucceeded()) {
                delete config;
                continue;
            }

            auto dconfig = config->config();
            QVERIFY2(dconfig != nullptr, "config() returned nullptr");
            QVERIFY2(dconfig->isValid(), "DConfig is not valid");

            // Trigger property change
            dconfig->setValue(QStringLiteral("themeName"),
                              QVariant::fromValue(QStringLiteral("affinity_test_%1").arg(cycle)));

            // Wait for signal
            int signalWait = 0;
            while (!signalReceived && signalWait < 100) {
                QCoreApplication::processEvents();
                QThread::msleep(5);
                signalWait++;
            }

            if (signalReceived) {
                QVERIFY2(signalThread == mainThread,
                         qPrintable(
                             QStringLiteral(
                                 "Signal emitted in wrong thread! Expected main thread %1, got %2")
                                 .arg(reinterpret_cast<quintptr>(mainThread))
                                 .arg(reinterpret_cast<quintptr>(signalThread))));
                qDebug() << "  Cycle" << cycle << "- Signal correctly emitted in main thread";
            } else {
                qDebug() << "  Cycle" << cycle << "- Signal not received (timeout)";
            }

            delete config;
            waitForCleanup();
        }

        qDebug() << "  Thread affinity verification completed";
    }

    // Test 3: Trigger property change then destroy
    void test_property_change_then_destroy()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 3: Property change then destroy ===";

        for (int cycle = 0; cycle < PROPERTY_CHANGE_CYCLES; ++cycle) {
            TreelandUserConfig *config = createConfig();

            QVERIFY2(
                config != nullptr,
                qPrintable(QStringLiteral("createByName returned nullptr in cycle %1").arg(cycle)));

            // Wait for initialization
            int waitCount = 0;
            while (!config->isInitializeSucceeded() && waitCount < 200) {
                QThread::msleep(INIT_TIMEOUT_MS);
                QCoreApplication::processEvents();
                waitCount++;
            }

            if (!config->isInitializeSucceeded()) {
                delete config;
                continue;
            }

            auto dconfig = config->config();
            QVERIFY2(dconfig != nullptr, "config() returned nullptr");
            QVERIFY2(dconfig->isValid(),
                     qPrintable(QStringLiteral("DConfig is not valid in cycle %1").arg(cycle)));

            qDebug() << "  Cycle" << cycle << "- DConfig is VALID";

            // Change a property
            dconfig->setValue(QStringLiteral("themeName"),
                              QVariant::fromValue(QStringLiteral("test_theme_%1").arg(cycle)));

            // Immediately delete
            delete config;
            config = nullptr;

            // Use standardized cleanup which matches the original safe cleanup
            waitForCleanup();
        }
    }

    // Test 4: Rapid property changes then destroy
    void test_rapid_changes_then_destroy()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 4: Rapid property changes then destroy ===";

        for (int cycle = 0; cycle < RAPID_CHANGE_CYCLES; ++cycle) {
            TreelandUserConfig *config = createConfig();

            QVERIFY2(
                config != nullptr,
                qPrintable(QStringLiteral("createByName returned nullptr in cycle %1").arg(cycle)));

            // Wait for initialization
            int waitCount = 0;
            while (!config->isInitializeSucceeded() && waitCount < 200) {
                QThread::msleep(INIT_TIMEOUT_MS);
                QCoreApplication::processEvents();
                waitCount++;
            }

            if (!config->isInitializeSucceeded()) {
                delete config;
                continue;
            }

            auto dconfig = config->config();
            QVERIFY2(dconfig != nullptr, "config() returned nullptr");
            QVERIFY2(dconfig->isValid(),
                     qPrintable(QStringLiteral("DConfig is not valid in cycle %1").arg(cycle)));

            // Rapidly change multiple properties
            for (int j = 0; j < 10; ++j) {
                dconfig->setValue(QStringLiteral("themeName"),
                                  QVariant::fromValue(QStringLiteral("theme_%1").arg(j)));
                dconfig->setValue(QStringLiteral("activeColor"),
                                  QVariant::fromValue(QStringLiteral("color_%1").arg(j)));
                QThread::msleep(1);
            }

            delete config;
            waitForCleanup(100, 1); // Slightly longer cleanup for rapid changes
        }
    }

    // Test 5: Multiple sequential operations
    void test_concurrent_configs()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 5: Multiple sequential config operations ===";

        for (int index = 1; index <= 3; ++index) {
            for (int cycle = 0; cycle < CONCURRENT_CONFIG_CYCLES; ++cycle) {
                TreelandUserConfig *config = createConfig();

                QVERIFY2(
                    config != nullptr,
                    qPrintable(QStringLiteral("createByName returned nullptr at index %1 cycle %2")
                                   .arg(index)
                                   .arg(cycle)));

                // Wait for initialization
                int waitCount = 0;
                while (!config->isInitializeSucceeded() && waitCount < 100) {
                    QThread::msleep(5);
                    QCoreApplication::processEvents();
                    waitCount++;
                }

                if (!config->isInitializeSucceeded()) {
                    delete config;
                    continue;
                }

                auto dconfig = config->config();
                QVERIFY2(dconfig != nullptr, "config() returned nullptr");
                QVERIFY2(dconfig->isValid(),
                         qPrintable(QStringLiteral("DConfig is not valid at index %1").arg(index)));

                dconfig->setValue(QStringLiteral("themeName"),
                                  QVariant::fromValue(QStringLiteral("seq_%1").arg(index)));

                delete config;
                waitForCleanup();
            }
        }

        waitForCleanup(100, 1);
    }

    // Test 6: Stress test
    void test_high_frequency_stress()
    {
        if (!s_dConfigAvailable) {
            QSKIP("DConfig schema not available on this system");
        }

        qDebug() << "\n=== Test 6: High frequency stress test ===";

        bool ok = false;
        int cycles = qEnvironmentVariableIntValue("TREELAND_STRESS_CYCLES", &ok);
        if (!ok || cycles <= 0) {
            cycles = DEFAULT_STRESS_CYCLES;
        }

        qDebug() << "  Running stress test with" << cycles
                 << "cycles (set TREELAND_STRESS_CYCLES to override)";

        auto start = std::chrono::high_resolution_clock::now();

        for (int cycle = 0; cycle < cycles; ++cycle) {
            TreelandUserConfig *config = createConfig();

            if (!config) {
                continue;
            }

            // Minimal wait
            QThread::msleep(1);
            QCoreApplication::processEvents();

            delete config;

            // Minimal cleanup for stress test
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        qDebug() << "  " << cycles << "cycles completed in" << duration.count() << "ms";
    }

    void cleanupTestCase()
    {
        waitForCleanup(200, 0);
        qDebug() << "=== Test Complete ===";
    }
};

bool ValidConfigCrashTest::s_dConfigAvailable = false;
bool ValidConfigCrashTest::s_initialized = false;

#include "test_dconfig_valid_config.moc"

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && !qEnvironmentVariableIsSet("DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QGuiApplication app(argc, argv);
    ValidConfigCrashTest tc;
    return QTest::qExec(&tc, argc, argv);
}
