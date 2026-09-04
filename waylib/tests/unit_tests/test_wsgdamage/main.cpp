// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wregionhighlightitem.h"
#include "wsgcontext_p.h"
#include "wrenderbuffernode_p.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTransform>
#include <QProcess>
#include <QQuickItem>
#include <QTest>

#include <memory>

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

    node->applyFrame(QRegion(), QRect(0, 0, 100, 40), QMatrix4x4());
    QVERIFY(!node->needsSourceCopy());
    QVERIFY(!node->needsRender(QRegion(), false));
    QVERIFY(node->needsRender(QRegion(QRect(0, 0, 100, 40)), false));
    QVERIFY(node->needsRender(QRegion(QRect(50, 0, 10, 10)), false));
    QVERIFY(!node->needsRender(QRegion(QRect(200, 0, 10, 10)), false));
    QVERIFY(node->needsRender(QRegion(), true));

    node->setDamageExpansion(8);
    node->setClipDamageExpansion(false);
    QVERIFY(node->needsRender(QRegion(QRect(-4, 0, 2, 2)), false));
    node->setClipDamageExpansion(true);
    QVERIFY(!node->needsRender(QRegion(QRect(-4, 0, 2, 2)), false));

    node->applyFrame(QRegion(QRect(0, 0, 100, 40)), QRect(0, 0, 100, 40), QMatrix4x4());
    QVERIFY(node->needsSourceCopy());
    QVERIFY(node->needsRender(QRegion(), false));
}

void WSGDamageTrackerTest::highlight_recordsContentUntilFade()
{
    WRegionOverlay overlay;
    overlay.addFrame(QRegion(QRect(0, 0, 10, 10)), false, 0);
    QCOMPARE(overlay.entries().size(), 1);
    QVERIFY(overlay.needsAnotherFrame());

    overlay.addFrame(QRegion(), false, WRegionOverlay::fadeOutMs - 1);
    QCOMPARE(overlay.entries().size(), 1);

    overlay.addFrame(QRegion(), false, WRegionOverlay::fadeOutMs);
    QVERIFY(overlay.entries().isEmpty());
    QVERIFY(!overlay.needsAnotherFrame());
}

void WSGDamageTrackerTest::highlight_newerOverlapRemovesOlder()
{
    WRegionOverlay debug;
    debug.addFrame(QRegion(QRect(0, 0, 20, 20)), false, 0);
    debug.addFrame(QRegion(QRect(0, 0, 10, 10)), false, 10);

    QCOMPARE(debug.entries().size(), 2);
    QCOMPARE(debug.entries().at(0).region, QRegion(QRect(0, 0, 10, 10)));
    QCOMPARE(debug.entries().at(1).region, QRegion(QRect(0, 0, 20, 20)) - QRect(0, 0, 10, 10));
}


void WSGDamageTrackerTest::highlight_olderFrameIsGreener()
{
    QList<WRegionOverlay::Entry> entries;
    WRegionOverlay::Entry newer;
    newer.region = QRegion(QRect(2, 2, 12, 12));
    newer.whenMs = 10;
    WRegionOverlay::Entry older;
    older.region = QRegion(QRect(22, 2, 12, 12));
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



int runDamageSceneTests(int argc, char **argv);

int runDamageTests(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "rhi");
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND"))
        qputenv("QSG_RHI_BACKEND", "null");
    QGuiApplication app(argc, argv);
    WSGContext::ensureInstalled();

    int status = 0;
    {
        WSGDamageTrackerTest tracker;
        status |= QTest::qExec(&tracker, argc, argv);
    }
    status |= runDamageSceneTests(argc, argv);
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
