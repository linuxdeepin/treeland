// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wregionhighlightitem.h"
#include "wsgcontext_p.h"
#include "wrenderbuffernode_p.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QMetaMethod>
#include <QPainter>
#include <QSet>
#include <QTransform>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQuickItem>
#include <QTest>

#include <memory>
#include <vector>

WAYLIB_SERVER_USE_NAMESPACE

class WSGDamageTrackerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void renderBuffer_needsRender_skipsIdleUnlessPendingHits();
    void highlight_recordsContentUntilFade();
    void highlight_newerOverlapRemovesOlder();
    void highlight_olderFrameIsGreener();
};

void WSGDamageTrackerTest::renderBuffer_needsRender_skipsIdleUnlessPendingHits()
{
    QQuickItem item;
    item.setSize(QSizeF(100, 40));
    std::unique_ptr<WRenderBufferNode> node(WRenderBufferNode::createSoftwareNode(&item));
    node->resize(item.size());

    node->applyFrame(WPixmanRegion(), QRect(0, 0, 100, 40), QMatrix4x4());
    QVERIFY(!node->needsSourceCopy());
    QVERIFY(!node->needsRender(WPixmanRegion(), false));
    QVERIFY(node->needsRender(WPixmanRegion(QRect(0, 0, 100, 40)), false));
    QVERIFY(node->needsRender(WPixmanRegion(QRect(50, 0, 10, 10)), false));
    QVERIFY(!node->needsRender(WPixmanRegion(QRect(200, 0, 10, 10)), false));
    QVERIFY(node->needsRender(WPixmanRegion(), true));

    node->setDamageExpansion(8);
    node->setClipDamageExpansion(false);
    QVERIFY(node->needsRender(WPixmanRegion(QRect(-4, 0, 2, 2)), false));
    node->setClipDamageExpansion(true);
    QVERIFY(!node->needsRender(WPixmanRegion(QRect(-4, 0, 2, 2)), false));

    node->applyFrame(WPixmanRegion(QRect(0, 0, 100, 40)), QRect(0, 0, 100, 40), QMatrix4x4());
    QVERIFY(node->needsSourceCopy());
    QVERIFY(node->needsRender(WPixmanRegion(), false));
}

void WSGDamageTrackerTest::highlight_recordsContentUntilFade()
{
    WRegionOverlay overlay;
    overlay.addFrame(WPixmanRegion(QRect(0, 0, 10, 10)), false, 0);
    QCOMPARE(overlay.entries().size(), 1);
    QVERIFY(overlay.needsAnotherFrame());

    overlay.addFrame(WPixmanRegion(), false, WRegionOverlay::fadeOutMs - 1);
    QCOMPARE(overlay.entries().size(), 1);

    overlay.addFrame(WPixmanRegion(), false, WRegionOverlay::fadeOutMs);
    QVERIFY(overlay.entries().isEmpty());
    QVERIFY(!overlay.needsAnotherFrame());
}

void WSGDamageTrackerTest::highlight_newerOverlapRemovesOlder()
{
    WRegionOverlay debug;
    debug.addFrame(WPixmanRegion(QRect(0, 0, 20, 20)), false, 0);
    debug.addFrame(WPixmanRegion(QRect(0, 0, 10, 10)), false, 10);

    QCOMPARE(debug.entries().size(), 2);
    QCOMPARE(debug.entries().at(0).region, WPixmanRegion(QRect(0, 0, 10, 10)));
    QCOMPARE(debug.entries().at(1).region, WPixmanRegion(QRect(0, 0, 20, 20)) - QRect(0, 0, 10, 10));
}


void WSGDamageTrackerTest::highlight_olderFrameIsGreener()
{
    QList<WRegionOverlay::Entry> entries;
    WRegionOverlay::Entry newer;
    newer.region = WPixmanRegion(QRect(2, 2, 12, 12));
    newer.whenMs = 10;
    WRegionOverlay::Entry older;
    older.region = WPixmanRegion(QRect(22, 2, 12, 12));
    older.whenMs = 0;
    entries << newer << older;

    QImage img(40, 20, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QPainter painter(&img);
    WRegionOverlay::paint(&painter, QTransform(), entries, 10);
    painter.end();

    const QRgb newPx = img.pixel(8, 8);
    const QRgb oldPx = img.pixel(28, 8);
    QVERIFY(qRed(newPx) > qGreen(newPx));
    QVERIFY(qGreen(oldPx) > qRed(oldPx));
    QVERIFY(qRed(newPx) > qRed(oldPx));
    QVERIFY(qGreen(oldPx) > qGreen(newPx));
}

QStringList damageSceneTestFunctionNames();

namespace {

QSet<QString> testSlots(const QMetaObject *meta)
{
    QSet<QString> names;
    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
        const QMetaMethod method = meta->method(i);
        if (method.methodType() == QMetaMethod::Slot)
            names.insert(QString::fromLatin1(method.name()));
    }
    return names;
}

// QTest options that consume the following argv element as a value.
bool optionTakesValue(const QString &option)
{
    static const QSet<QString> valued{
        QStringLiteral("-o"), QStringLiteral("-outdir"),
        QStringLiteral("-eventdelay"), QStringLiteral("-keydelay"),
        QStringLiteral("-mousedelay"), QStringLiteral("-maxwarnings"),
        QStringLiteral("-randomseed"),
    };
    return valued.contains(option);
}

struct SuiteArgs
{
    std::vector<QByteArray> storage;
    std::vector<char *> argv;
};

SuiteArgs suiteArgs(const QString &program, const QStringList &qtOptions,
                    const QStringList &functions)
{
    SuiteArgs out;
    out.storage.reserve(1 + qtOptions.size() + functions.size());
    out.storage.push_back(program.toLocal8Bit());
    for (const QString &option : qtOptions)
        out.storage.push_back(option.toLocal8Bit());
    for (const QString &function : functions)
        out.storage.push_back(function.toLocal8Bit());
    out.argv.reserve(out.storage.size());
    for (QByteArray &blob : out.storage)
        out.argv.push_back(blob.data());
    return out;
}

} // namespace

int runDamageSceneTests(int argc, char **argv);

int runDamageTests(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "rhi");
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND"))
        qputenv("QSG_RHI_BACKEND", "null");
    QGuiApplication app(argc, argv);
    WSGContext::ensureInstalled();

    // Split CLI selectors per suite: forwarding every positional filter to
    // both qExec calls makes the non-owning suite fail with "Unknown test
    // function". Qt options go to both; function names only to their owner.
    QStringList qtOptions;
    QStringList positionals;
    const QStringList cli = app.arguments().mid(1);
    for (int i = 0; i < cli.size(); ++i) {
        if (cli.at(i).startsWith(QLatin1Char('-'))) {
            qtOptions << cli.at(i);
            if (optionTakesValue(cli.at(i)) && i + 1 < cli.size())
                qtOptions << cli.at(++i);
        } else {
            positionals << cli.at(i);
        }
    }
    const QSet<QString> trackerSlots = testSlots(&WSGDamageTrackerTest::staticMetaObject);
    const QStringList sceneNames = damageSceneTestFunctionNames();
    const QSet<QString> sceneSlots(sceneNames.begin(), sceneNames.end());
    QStringList trackerFunctions;
    QStringList sceneFunctions;
    bool trackerOwns = false;
    bool sceneOwns = false;
    bool anyUnowned = false;
    for (const QString &function : positionals) {
        const bool inTracker = trackerSlots.contains(function);
        const bool inScene = sceneSlots.contains(function);
        if (inTracker && !inScene) {
            trackerFunctions << function;
            trackerOwns = true;
        } else if (inScene && !inTracker) {
            sceneFunctions << function;
            sceneOwns = true;
        } else {
            // Shared (init/cleanup) or unknown: both suites see it, exactly
            // like the old shared-argv behavior.
            trackerFunctions << function;
            sceneFunctions << function;
            if (!inTracker && !inScene)
                anyUnowned = true;
        }
    }

    // A selector owned by one suite must not run the other suite's full set.
    const bool runTracker = positionals.isEmpty() || trackerOwns || anyUnowned;
    const bool runScene = positionals.isEmpty() || sceneOwns || anyUnowned;
    const QString program = app.arguments().constFirst();
    int status = 0;
    if (runTracker) {
        WSGDamageTrackerTest tracker;
        SuiteArgs args = suiteArgs(program, qtOptions, trackerFunctions);
        status |= QTest::qExec(&tracker, int(args.argv.size()), args.argv.data());
    }
    if (runScene) {
        SuiteArgs args = suiteArgs(program, qtOptions, sceneFunctions);
        status |= runDamageSceneTests(int(args.argv.size()), args.argv.data());
    }
    return status;
}

int main(int argc, char **argv)
{
    if (!qEnvironmentVariableIsSet("WSG_DAMAGE_TEST_CHILD")
        && !qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        QCoreApplication app(argc, argv);
        int status = 0;
        const QStringList args = app.arguments().mid(1);
        const char *backends[] = { "null", "opengl", "vulkan" };
        for (const char *backend : backends) {
            QProcess process;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("QSG_RHI_BACKEND"), QLatin1String(backend));
            env.insert(QStringLiteral("WSG_DAMAGE_TEST_CHILD"), QStringLiteral("1"));
            process.setProcessEnvironment(env);
            process.setProcessChannelMode(QProcess::ForwardedChannels);
            fprintf(stderr, "\n===== QSG_RHI_BACKEND=%s =====\n", backend);
            process.start(app.applicationFilePath(), args);
            if (!process.waitForStarted(15000) || !process.waitForFinished(-1)) {
                fprintf(stderr, "QSG_RHI_BACKEND=%s failed to run: %s\n",
                        backend, qPrintable(process.errorString()));
                return 1;
            }
            const int code = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : 1;
            fprintf(stderr, "===== QSG_RHI_BACKEND=%s exit %d =====\n", backend, code);
            status |= code;
        }
        return status;
    }
    return runDamageTests(argc, argv);
}

#include "main.moc"
