// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "canvaswindow.h"
#include "capture.h"

#include <private/qwaylandwindow_p.h>

#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    TreelandCaptureManager manager;
    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    auto captureWithMask = [&app, &manager](::wl_surface *mask) {
        auto captureContext = manager.getContext();
        if (!captureContext) {
            app.exit(-1);
        }
        captureContext->selectSource(0x1 | 0x2 | 0x4, true, false, mask);
        QEventLoop loop;
        QObject::connect(captureContext,
                         &TreelandCaptureContext::sourceReady,
                         &loop,
                         &QEventLoop::quit);
        loop.exec();
        auto frame = captureContext->frame();
        QImage result;
        QObject::connect(frame, &TreelandCaptureFrame::ready, &app, [&result, &loop](QImage image) {
            result = image;
            loop.quit();
        });
        QObject::connect(frame, &TreelandCaptureFrame::failed, &app, [&loop] {
            loop.quit();
        });
        loop.exec();
        if (result.isNull())
            app.exit(-1);
        auto saveBasePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        QDir saveBaseDir(saveBasePath);
        if (!saveBaseDir.exists())
            app.exit(-1);
        QString picName = "portal screenshot - " + QDateTime::currentDateTime().toString() + ".png";
        if (result.save(saveBaseDir.absoluteFilePath(picName), "PNG")) {
            qDebug() << saveBaseDir.absoluteFilePath(picName);
            app.quit();
        } else {
            app.exit(-1);
        }
    };
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &manager,
        [&app, &manager, captureWithMask](QObject *object, const QUrl &url) {
            if (auto canvasWindow = qobject_cast<CanvasWindow *>(object)) {
                auto waylandWindow =
                    static_cast<QtWaylandClient::QWaylandWindow *>(canvasWindow->handle());
                if (manager.isActive()) {
                    captureWithMask(waylandWindow->surface());
                } else {
                    QObject::connect(&manager,
                                     &TreelandCaptureManager::activeChanged,
                                     &manager,
                                     std::bind(captureWithMask, waylandWindow->surface()));
                }
            }
        });
    engine.loadFromModule("capture", "Main");
    return app.exec();
}
