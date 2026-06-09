// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTest>

#include <functional>

#include "dummyddmstate.h"

namespace {
QString buildRootPath()
{
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral(".."));
}

QString dummyDdmToolPath()
{
    return QDir::cleanPath(buildRootPath()
                           + QStringLiteral("/tools/treeland-ddm-debug/treeland-ddm-debug"));
}

QString treelandBinaryPath()
{
    return QDir::cleanPath(buildRootPath() + QStringLiteral("/src/treeland"));
}

QString testDataPath(const QString &name)
{
    const QByteArray relativePath = QByteArray("testdata/") + name.toUtf8();
    return QTest::qFindTestData(relativePath.constData(), __FILE__, __LINE__);
}

QString uniqueDdmUrl()
{
    static int counter = 0;
    return QStringLiteral("local:org.deepin.dde.ddm.qro.test.%1.%2")
        .arg(QCoreApplication::applicationPid())
        .arg(++counter);
}

bool waitForLog(QProcess &process, const QByteArray &message, int timeoutMs = 10000)
{
    QByteArray output = process.readAll();
    if (output.contains(message))
        return true;

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (!process.waitForReadyRead(100))
            continue;

        output += process.readAll();
        if (output.contains(message))
            return true;
    }

    return false;
}

void stopProcess(QProcess &process)
{
    if (process.state() == QProcess::NotRunning)
        return;

    process.terminate();
    if (!process.waitForFinished(3000)) {
        process.kill();
        process.waitForFinished(3000);
    }
}

bool startDummyService(QProcess &service, const QString &ddmUrl, const QString &stateFile)
{
    service.setProcessChannelMode(QProcess::MergedChannels);
    service.setProgram(dummyDdmToolPath());
    service.setArguments({
        QStringLiteral("service"),
        QStringLiteral("--ddm-url"), ddmUrl,
        QStringLiteral("--treeland-url"), QStringLiteral("local:org.deepin.dde.treeland.qro"),
        QStringLiteral("--state"), stateFile,
        QStringLiteral("--verbose")
    });
    service.start();
    return service.waitForStarted() && waitForLog(service, QByteArrayLiteral("DDMRemote ready on"));
}

bool startTreeland(QProcess &treeland, const QString &ddmUrl)
{
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TREELAND_DDM_REMOTE_URL"), ddmUrl);
    if (!env.contains(QStringLiteral("WAYLAND_DISPLAY")) && !env.contains(QStringLiteral("DISPLAY"))) {
        env.insert(QStringLiteral("WLR_BACKENDS"), QStringLiteral("headless"));
        env.insert(QStringLiteral("WLR_RENDERER"), QStringLiteral("pixman"));
        env.insert(QStringLiteral("WLR_LIBINPUT_NO_DEVICES"), QStringLiteral("1"));
    }
    treeland.setProcessEnvironment(env);
    treeland.setProcessChannelMode(QProcess::MergedChannels);
    treeland.setProgram(treelandBinaryPath());
    treeland.start();
    if (!treeland.waitForStarted()
        || !waitForLog(treeland,
                       QByteArrayLiteral("DDM remote object is listening on local:org.deepin.dde.treeland.qro"))) {
        return false;
    }

    QTest::qWait(3000);
    return true;
}

QByteArray runCtlCapture(const QString &ddmUrl, const QStringList &commandArgs, int *exitCode = nullptr)
{
    QProcess ctl;
    ctl.setProcessChannelMode(QProcess::MergedChannels);
    ctl.setProgram(dummyDdmToolPath());
    ctl.setArguments(QStringList{ QStringLiteral("ctl"), QStringLiteral("--ddm-url"), ddmUrl } + commandArgs);
    ctl.start();
    if (!ctl.waitForFinished(10000)) {
        ctl.kill();
        ctl.waitForFinished(3000);
    }
    if (exitCode)
        *exitCode = ctl.exitCode();
    return ctl.readAll();
}

QJsonObject statusObject(const QString &ddmUrl)
{
    int exitCode = -1;
    const QByteArray output = runCtlCapture(ddmUrl, { QStringLiteral("status") }, &exitCode);
    if (exitCode != 0)
        return {};
    return QJsonDocument::fromJson(output).object();
}

bool waitForStatus(const QString &ddmUrl,
                   const std::function<bool(const QJsonObject &)> &predicate,
                   int timeoutMs = 10000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        const QJsonObject status = statusObject(ddmUrl);
        if (!status.isEmpty() && predicate(status))
            return true;
        QTest::qWait(100);
    }
    return false;
}
}

class TestDummyDdmGreeter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void usesConfiguredDdmRemoteUrl();
    void loadsDummyStateFromJson();
    void ctlStatusReportsCurrentState();
    void ctlLockTriggersTreelandLock();
    void ctlUnlockUpdatesGreeterState();
    void ctlSwitchUserChangesTreelandView();
    void ctlAddsActiveUserSession();
    void preloadedActiveSessionsReplayToGreeter();
    void ctlSetSessionsUpdatesStatus();
};

void TestDummyDdmGreeter::usesConfiguredDdmRemoteUrl()
{
    const QString ddmUrl = uniqueDdmUrl();
    QProcess service;
    QVERIFY2(startDummyService(service, ddmUrl, testDataPath(QStringLiteral("basic-state.json"))),
             "dummy ddm service did not become ready");

    QProcess treeland;
    QVERIFY2(startTreeland(treeland, ddmUrl), "treeland did not start");
    QVERIFY2(waitForLog(treeland, QByteArrayLiteral("Connecting to DDM remote node: QUrl(\"local:org.deepin.dde.ddm.qro.test")),
             "treeland did not attempt to connect using the overridden DDM URL");

    stopProcess(treeland);
    stopProcess(service);
}

void TestDummyDdmGreeter::loadsDummyStateFromJson()
{
    DummyDdmState state;
    QVERIFY2(state.loadFromFile(testDataPath(QStringLiteral("basic-state.json"))),
             "failed to load dummy ddm state fixture");
    QCOMPARE(state.lastUser, QStringLiteral("alice"));
    QCOMPARE(state.lastSession, QStringLiteral("treeland.desktop"));
    QVERIFY(state.rememberLastSession);
    QCOMPARE(state.sessions.size(), 2);
    QCOMPARE(state.sessions.front().fileName, QStringLiteral("treeland.desktop"));
}

void TestDummyDdmGreeter::ctlStatusReportsCurrentState()
{
    const QString ddmUrl = uniqueDdmUrl();
    QProcess service;
    QVERIFY2(startDummyService(service, ddmUrl, testDataPath(QStringLiteral("basic-state.json"))),
             "dummy ddm service did not become ready");

    const QJsonObject status = statusObject(ddmUrl);
    QCOMPARE(status.value(QStringLiteral("lastUser")).toString(), QStringLiteral("alice"));
    QCOMPARE(status.value(QStringLiteral("lastSession")).toString(), QStringLiteral("treeland.desktop"));

    stopProcess(service);
}

void TestDummyDdmGreeter::ctlLockTriggersTreelandLock()
{
    const QString ddmUrl = uniqueDdmUrl();
    QProcess service;
    QVERIFY2(startDummyService(service, ddmUrl, testDataPath(QStringLiteral("basic-state.json"))),
             "dummy ddm service did not become ready");

    QProcess treeland;
    QVERIFY2(startTreeland(treeland, ddmUrl), "treeland did not start");

    int exitCode = -1;
    runCtlCapture(ddmUrl, { QStringLiteral("lock") }, &exitCode);
    QCOMPARE(exitCode, 0);
    QVERIFY2(waitForLog(treeland, QByteArrayLiteral("DDM remote: lock")), "treeland did not receive lock request");
    QVERIFY2(waitForStatus(ddmUrl, [](const QJsonObject &status) {
                 return status.value(QStringLiteral("treelandLocked")).toBool();
             }),
             "dummy ddm state did not observe treeland locked state");

    stopProcess(treeland);
    stopProcess(service);
}

void TestDummyDdmGreeter::ctlUnlockUpdatesGreeterState()
{
    const QString ddmUrl = uniqueDdmUrl();
    QProcess service;
    QVERIFY2(startDummyService(service, ddmUrl, testDataPath(QStringLiteral("basic-state.json"))),
             "dummy ddm service did not become ready");

    QProcess treeland;
    QVERIFY2(startTreeland(treeland, ddmUrl), "treeland did not start");
    QVERIFY2(waitForLog(service, QByteArrayLiteral("sessions")),
             "greeter did not finish initial DDM session sync");

    const QString currentUser = qEnvironmentVariable("USER");
    QVERIFY2(!currentUser.isEmpty(), "USER environment variable is empty");

    int exitCode = -1;
    runCtlCapture(ddmUrl, { QStringLiteral("set-current-user"), currentUser }, &exitCode);
    QCOMPARE(exitCode, 0);
    runCtlCapture(ddmUrl, { QStringLiteral("set-last-user"), currentUser }, &exitCode);
    QCOMPARE(exitCode, 0);

    runCtlCapture(ddmUrl, { QStringLiteral("lock") }, &exitCode);
    QCOMPARE(exitCode, 0);
    QVERIFY2(waitForStatus(ddmUrl, [](const QJsonObject &status) {
                 return status.value(QStringLiteral("treelandLocked")).toBool();
             }),
             "treeland never entered locked state");

    runCtlCapture(ddmUrl, { QStringLiteral("unlock") }, &exitCode);
    QCOMPARE(exitCode, 0);
    QVERIFY2(waitForLog(treeland, QByteArrayLiteral("User session added:")),
             "treeland did not receive unlock session signal");
    QVERIFY2(waitForStatus(ddmUrl, [currentUser](const QJsonObject &status) {
                 const auto sessions = status.value(QStringLiteral("activeSessions")).toArray();
                 for (const auto &session : sessions) {
                     const auto object = session.toObject();
                     if (object.value(QStringLiteral("user")).toString() == currentUser)
                         return true;
                 }
                 return false;
             }),
             "dummy ddm state did not retain the unlocked active session");

    stopProcess(treeland);
    stopProcess(service);
}

void TestDummyDdmGreeter::ctlSwitchUserChangesTreelandView()
{
    const QString ddmUrl = uniqueDdmUrl();
    QProcess service;
    QVERIFY2(startDummyService(service, ddmUrl, testDataPath(QStringLiteral("multi-user-state.json"))),
             "dummy ddm service did not become ready");

    QProcess treeland;
    QVERIFY2(startTreeland(treeland, ddmUrl), "treeland did not start");

    int exitCode = -1;
    runCtlCapture(ddmUrl, { QStringLiteral("switch-user"), QStringLiteral("bob") }, &exitCode);
    QCOMPARE(exitCode, 0);
    QVERIFY2(waitForLog(treeland, QByteArrayLiteral("DDM remote: switchToUser")),
             "treeland did not receive switch-user request");

    stopProcess(treeland);
    stopProcess(service);
}

void TestDummyDdmGreeter::ctlAddsActiveUserSession()
{
    const QString ddmUrl = uniqueDdmUrl();
    QProcess service;
    QVERIFY2(startDummyService(service, ddmUrl, testDataPath(QStringLiteral("multi-user-state.json"))),
             "dummy ddm service did not become ready");

    QProcess treeland;
    QVERIFY2(startTreeland(treeland, ddmUrl), "treeland did not start");
    QVERIFY2(waitForLog(service, QByteArrayLiteral("sessions")),
             "greeter did not finish initial DDM session sync");

    int exitCode = -1;
    runCtlCapture(ddmUrl, { QStringLiteral("add-user-session"), QStringLiteral("bob"), QStringLiteral("42") }, &exitCode);
    QCOMPARE(exitCode, 0);
    QVERIFY2(waitForLog(treeland, QByteArrayLiteral("User session added:")),
             "treeland did not observe active user session update");
    QVERIFY2(waitForStatus(ddmUrl, [](const QJsonObject &status) {
                 const auto sessions = status.value(QStringLiteral("activeSessions")).toArray();
                 for (const auto &session : sessions) {
                     const auto object = session.toObject();
                     if (object.value(QStringLiteral("user")).toString() == QStringLiteral("bob")
                         && object.value(QStringLiteral("sessionId")).toInt() == 42) {
                         return true;
                     }
                 }
                 return false;
             }),
             "dummy ddm state did not retain the new active user session");

    stopProcess(treeland);
    stopProcess(service);
}

void TestDummyDdmGreeter::preloadedActiveSessionsReplayToGreeter()
{
    const QString ddmUrl = uniqueDdmUrl();
    QProcess service;
    QVERIFY2(startDummyService(service, ddmUrl, testDataPath(QStringLiteral("multi-user-state.json"))),
             "dummy ddm service did not become ready");

    QProcess treeland;
    QVERIFY2(startTreeland(treeland, ddmUrl), "treeland did not start");
    QVERIFY2(waitForLog(service, QByteArrayLiteral("connectGreeter")),
             "dummy ddm service did not observe greeter connect");
    QVERIFY2(waitForLog(treeland, QByteArrayLiteral("User session added: id= 10")),
             "treeland did not replay preloaded active user sessions");

    stopProcess(treeland);
    stopProcess(service);
}

void TestDummyDdmGreeter::ctlSetSessionsUpdatesStatus()
{
    const QString ddmUrl = uniqueDdmUrl();
    QProcess service;
    QVERIFY2(startDummyService(service, ddmUrl, testDataPath(QStringLiteral("basic-state.json"))),
             "dummy ddm service did not become ready");

    QProcess treeland;
    QVERIFY2(startTreeland(treeland, ddmUrl), "treeland did not start");
    QVERIFY2(waitForLog(service, QByteArrayLiteral("sessions")),
             "greeter did not finish initial DDM session sync");

    int exitCode = -1;
    runCtlCapture(ddmUrl, { QStringLiteral("set-sessions"), testDataPath(QStringLiteral("single-session-state.json")) }, &exitCode);
    QCOMPARE(exitCode, 0);
    QVERIFY2(waitForLog(treeland, QByteArrayLiteral("Refreshed sessions from DDM: 1")),
             "running greeter did not refresh session list after set-sessions");
    QVERIFY2(waitForStatus(ddmUrl, [](const QJsonObject &status) {
                 return status.value(QStringLiteral("sessions")).toArray().size() == 1;
             }),
             "dummy ddm state did not publish the updated session list");

    stopProcess(treeland);
    stopProcess(service);
}

QTEST_MAIN(TestDummyDdmGreeter)
#include "test_dummy_ddm_greeter.moc"
