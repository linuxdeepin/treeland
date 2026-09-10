// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wbackend.h>
#include <woutput.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wrenderhelper.h>
#include <wserver.h>
#include <wlr_all.h>

extern "C" {
#include <wlr/interfaces/wlr_output.h>
}

#include <QGuiApplication>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>

WAYLIB_SERVER_USE_NAMESPACE

class OutputFrameTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nativeFramesOnlyCommitTheirOutput()
    {
        WServer server;
        auto *backend = server.attach<WBackend>();
        server.start();
        auto *renderer = WRenderHelper::createRenderer(backend->handle());
        QVERIFY(renderer);
        const auto rendererCleanup = qScopeGuard([&] { wlr_renderer_destroy(renderer); });
        auto *allocator = wlr_allocator_autocreate(backend->handle(), renderer);
        QVERIFY(allocator);
        const auto allocatorCleanup = qScopeGuard([&] { wlr_allocator_destroy(allocator); });
        QVERIFY(wlr_backend_start(backend->handle()));
        const auto outputs = backend->outputList();
        QCOMPARE(outputs.size(), 2);

        WOutputRenderWindow window;
        window.setWidth(128);
        window.setHeight(64);
        window.init(renderer, allocator);
        WOutputViewport a(window.contentItem());
        WOutputViewport b(window.contentItem());
        a.setOutput(outputs[0]);
        b.setOutput(outputs[1]);
        for (int i = 0; i < outputs.size(); ++i) {
            wlr_output_state state;
            wlr_output_state_init(&state);
            const auto stateCleanup = qScopeGuard([&] { wlr_output_state_finish(&state); });
            wlr_output_state_set_custom_mode(&state, 64, 64, i == 0 ? 60000 : 144000);
            wlr_output_state_set_enabled(&state, true);
            QVERIFY(wlr_output_commit_state(outputs[i]->handle(), &state));
        }

        QSignalSpy rendered(&window, &WOutputRenderWindow::renderEnd);
        auto prepare = [&] {
            window.update();
            // Both outputs are dirty and ready. Emit frames explicitly rather
            // than dispatching the headless backend's refresh timers.
            for (auto *output : outputs)
                output->handle()->frame_pending = false;
            rendered.clear();
        };

        prepare();
        wlr_output_send_frame(outputs[0]->handle());
        QCOMPARE(rendered.size(), 1);
        QCOMPARE(qvariant_cast<QList<QPointer<WOutput>>>(rendered.takeFirst().first()),
                 QList<QPointer<WOutput>>{outputs[0]});

        prepare();
        wlr_output_send_frame(outputs[1]->handle());
        QCOMPARE(rendered.size(), 1);
        QCOMPARE(qvariant_cast<QList<QPointer<WOutput>>>(rendered.takeFirst().first()),
                 QList<QPointer<WOutput>>{outputs[1]});

        prepare();
        window.render();
        QCOMPARE(rendered.size(), 1);
        const auto committed = qvariant_cast<QList<QPointer<WOutput>>>(rendered.takeFirst().first());
        QCOMPARE(committed.size(), 2);
        QVERIFY(committed.contains(outputs[0]));
        QVERIFY(committed.contains(outputs[1]));
    }
};

int main(int argc, char **argv)
{
    qputenv("WLR_BACKENDS", "headless");
    qputenv("WLR_HEADLESS_OUTPUTS", "2");
    qputenv("QT_QUICK_BACKEND", "software");
    WServer::initializeQPA();
    QGuiApplication app(argc, argv);
    OutputFrameTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "main.moc"
