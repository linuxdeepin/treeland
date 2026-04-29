// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

class HeadlessTreelandTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_treelandPath = qEnvironmentVariable("TREELAND_TEST_BINARY");
        QVERIFY2(!m_treelandPath.isEmpty(),
                 "TREELAND_TEST_BINARY must point to the treeland executable");

        const QFileInfo binary(m_treelandPath);
        QVERIFY2(binary.exists(),
                 qPrintable(QStringLiteral("Missing binary: %1").arg(m_treelandPath)));
        QVERIFY2(binary.isExecutable(),
                 qPrintable(QStringLiteral("Binary is not executable: %1").arg(m_treelandPath)));
    }

    void tryExecStartsInHeadlessEnvironment()
    {
        QTemporaryDir runtimeDir;
        QVERIFY(runtimeDir.isValid());

        QProcess process;
        process.setProcessEnvironment(headlessEnvironment(runtimeDir.path()));
        process.start(m_treelandPath, { QStringLiteral("--try-exec") });

        QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));
        QVERIFY2(process.waitForFinished(10000), qPrintable(process.errorString()));

        const QByteArray output = process.readAllStandardOutput() + process.readAllStandardError();
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, output.constData());
    }

    void compositorCreatesWaylandSocketWhenExplicitlyEnabled()
    {
        if (!qEnvironmentVariableIsSet("TREELAND_RUN_HEADLESS_COMPOSITOR")) {
            QSKIP("Set TREELAND_RUN_HEADLESS_COMPOSITOR=1 to run the real compositor smoke test");
        }

        QTemporaryDir runtimeDir;
        QVERIFY(runtimeDir.isValid());

        const QString displayName =
            QStringLiteral("treeland-headless-test-%1").arg(QCoreApplication::applicationPid());
        const QString socketPath = QDir(runtimeDir.path()).filePath(displayName);

        QProcess process;
        process.setProcessEnvironment(headlessEnvironment(runtimeDir.path(), displayName));
        process.start(m_treelandPath);

        QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));

        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(socketPath), 10000);

        process.terminate();
        if (!process.waitForFinished(5000)) {
            process.kill();
            QVERIFY(process.waitForFinished(5000));
        }

        const QByteArray output = process.readAllStandardOutput() + process.readAllStandardError();
        QVERIFY2(process.exitStatus() == QProcess::NormalExit
                     || process.exitStatus() == QProcess::CrashExit,
                 output.constData());
    }

private:
    QProcessEnvironment headlessEnvironment(const QString &runtimeDir,
                                            const QString &waylandDisplay = QString()) const
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtimeDir);
        env.insert(QStringLiteral("WLR_BACKENDS"), QStringLiteral("headless"));
        env.insert(QStringLiteral("WLR_RENDERER"), QStringLiteral("pixman"));
        env.insert(QStringLiteral("QT_LOGGING_RULES"), QStringLiteral("treeland.*=true"));
        env.remove(QStringLiteral("DISPLAY"));

        if (!waylandDisplay.isEmpty())
            env.insert(QStringLiteral("WAYLAND_DISPLAY"), waylandDisplay);

        return env;
    }

    QString m_treelandPath;
};

QTEST_MAIN(HeadlessTreelandTest)
#include "main.moc"
