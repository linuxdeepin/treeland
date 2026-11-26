// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "dconfig/userconfigbackend.h"
#include "testconfig.hpp"

#include <QDBusContext>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QProcess>
#include <QSignalSpy>
#include <QTest>

#include <cstdio>
#include <limits>

// A separate process serves an isolated bus. No system configuration is touched.
class MockManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.desktopspec.ConfigManager.Manager")
    Q_PROPERTY(QStringList keyList READ keyList)
public:
    using QObject::QObject;

    QStringList keyList() const
    {
        return { QStringLiteral("enabled"), QStringLiteral("payload") };
    }

    int releases = 0;

public Q_SLOTS:

    QDBusVariant value(const QString &key)
    {
        return QDBusVariant(values.value(
            key,
            key == QLatin1String("enabled") ? QVariant(true) : QVariant(QVariantMap{})));
    }

    void setValue(const QString &key, const QDBusVariant &value)
    {
        values.insert(key, value.variant());
        Q_EMIT valueChanged(key);
    }

    void reset(const QString &key)
    {
        values.remove(key);
        Q_EMIT valueChanged(key);
    }

    bool isDefaultValue(const QString &key)
    {
        return !values.contains(key);
    }

    QString permissions(const QString &)
    {
        return QStringLiteral("readwrite");
    }

    void release()
    {
        ++releases;
    }

Q_SIGNALS:
    void valueChanged(const QString &key);

private:
    QMap<QString, QVariant> values;
};

class MockServer
    : public QObject
    , protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.desktopspec.ConfigManager")
public Q_SLOTS:

    QDBusObjectPath acquireManagerV2(uint uid,
                                     const QString &appId,
                                     const QString &name,
                                     const QString &subpath)
    {
        if (uid == std::numeric_limits<uint>::max() || appId != QLatin1String("test.app")
            || name != QLatin1String("test-config") || subpath != QLatin1String("/same-app")) {
            sendErrorReply(QDBusError::Failed, QStringLiteral("Invalid acquisition"));
            return QDBusObjectPath();
        }
        const auto path = QStringLiteral("/manager/u%1").arg(uid);
        if (!managers.contains(path)) {
            auto *manager = new MockManager(this);
            managers.insert(path, manager);
            QDBusConnection::systemBus().registerObject(path,
                                                        manager,
                                                        QDBusConnection::ExportAllSlots
                                                            | QDBusConnection::ExportAllSignals
                                                            | QDBusConnection::ExportAllProperties);
        }
        return QDBusObjectPath(path);
    }

    int releaseCount(uint uid)
    {
        const auto *manager = managers.value(QStringLiteral("/manager/u%1").arg(uid));
        return manager ? manager->releases : 0;
    }

private:
    QMap<QString, MockManager *> managers;
};

class BackendTest : public QObject
{
    Q_OBJECT
private:
    QProcess daemon;

    TestConfig *create(uint uid)
    {
        auto *backend =
            new UserConfigBackend(uid, QStringLiteral("test-config"), QStringLiteral("/same-app"));
        auto *config = TestConfig::createByName(backend,
                                                QStringLiteral("test-config"),
                                                QStringLiteral("test.app"),
                                                QStringLiteral("/same-app"),
                                                this);
        connect(config,
                &TestConfig::configInitializeSucceed,
                backend,
                &UserConfigBackend::subscribe,
                Qt::SingleShotConnection);
        return config;
    }

private Q_SLOTS:

    void initTestCase()
    {
        daemon.start(QCoreApplication::applicationFilePath(), { QStringLiteral("--mock") });
        QVERIFY(daemon.waitForStarted());
        QVERIFY(daemon.waitForReadyRead());
        QCOMPARE(daemon.readLine().trimmed(), QByteArray("ready"));
    }

    void isolationNotificationsResetAndRelease()
    {
        auto *a = create(1000);
        auto *observer = create(1000);
        auto *b = create(1001);
        QTRY_VERIFY(a->isInitializeSucceeded());
        QTRY_VERIFY(observer->isInitializeSucceeded());
        QTRY_VERIFY(b->isInitializeSucceeded());
        // Wait for the subscription refresh, not just the optimistic local setter.
        QSignalSpy changed(observer, &TestConfig::enabledChanged);
        a->setEnabled(false);
        QTRY_COMPARE(observer->enabled(), false);
        QVERIFY(!changed.isEmpty());
        QCOMPARE(b->enabled(), true);

        // Exercise a nested D-Bus dictionary/array round trip through the backend.
        const QVariantMap payload{ { QStringLiteral("size"), QVariantList{ 720, 520 } },
                                   { QStringLiteral("nested"),
                                     QVariantMap{ { QStringLiteral("flag"), true } } } };
        a->setPayload(payload);
        QTRY_COMPARE(observer->payload(), payload);
        QCOMPARE(b->payload(), QVariantMap{});

        a->resetEnabled();
        QTRY_COMPARE(observer->enabled(), true);
        QTRY_COMPARE(a->enabled(), true);
        delete a;
        delete observer;
        delete b;
        QDBusInterface root(QStringLiteral("org.desktopspec.ConfigManager"),
                            QStringLiteral("/"),
                            QStringLiteral("org.desktopspec.ConfigManager"),
                            QDBusConnection::systemBus());
        const auto releaseCount = [&root](uint uid) {
            return QDBusReply<int>(root.call(QStringLiteral("releaseCount"), uid)).value();
        };
        QTRY_COMPARE(releaseCount(1000), 2);
        QTRY_COMPARE(releaseCount(1001), 1);
    }

    void acquisitionFailureDoesNotFallback()
    {
        auto *config = create(std::numeric_limits<uint>::max());
        QTRY_VERIFY(config->isInitializeFailed());
        QVERIFY(!config->isInitializeSucceeded());
        delete config;
    }

    void cleanupTestCase()
    {
        daemon.terminate();
        QVERIFY(daemon.waitForFinished());
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--mock"))) {
        MockServer server;
        auto bus = QDBusConnection::systemBus();
        if (!bus.registerService(QStringLiteral("org.desktopspec.ConfigManager"))
            || !bus.registerObject(QStringLiteral("/"), &server, QDBusConnection::ExportAllSlots))
            return 1;
        std::puts("ready");
        std::fflush(stdout);
        return app.exec();
    }
    BackendTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "main.moc"
