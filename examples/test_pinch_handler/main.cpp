// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    using namespace Qt::StringLiterals;
    const QUrl url(u"qrc:/qt/qml/pinchhandler/Main.qml"_s);
#else
    const QUrl url(u"qrc:/pinchhandler/Main.qml"_qs);
#endif
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
