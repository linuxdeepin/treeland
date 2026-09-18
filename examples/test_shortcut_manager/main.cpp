// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// This is a simple test application for the Treeland Shortcut Manager V3 protocol.
// It reads a JSON configuration file specifying a list of shortcut binding requests
// and sends them to the Treeland compositor via the Wayland protocol.
// The JSON file should contain an array of requests, each with the following format:
//    [
//        {
//            "req": "bind_key",
//            "name": "shortcut1",
//            "key": "Ctrl-A",
//            "mode": 1,
//            "action": 0
//        },
//        { ... },
//        ...
//    ]
//
// Note: In v3, the action enum values are shifted down by 1 (notify=0).
// Bind requests take effect immediately; no commit is needed.

#include <QGuiApplication>
#include <QObject>
#include <QScreen>
#include <QWaylandClientExtension>
#include <QCommandLineParser>
#include <QKeySequence>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <qjsonarray.h>

#include "qwayland-treeland-shortcut-manager-unstable-v3.h"

class ShortcutManagerV3
    : public QWaylandClientExtensionTemplate<ShortcutManagerV3>
    , public QtWayland::treeland_shortcut_manager_v3
{
    Q_OBJECT
public:
    explicit ShortcutManagerV3()
        : QWaylandClientExtensionTemplate<ShortcutManagerV3>(1)
    {

    }

    void treeland_shortcut_manager_v3_activated(const QString &name, uint repeat) override
    {
        qInfo() << "shortcut activated: " << name << " repeat: " << repeat;
    }

    void treeland_shortcut_manager_v3_bind_failure(const QString &name, uint error) override
    {
        qWarning() << "bind failure:" << name << "error:" << error;
    }
};


int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("Test Shortcut Manager V3");
    parser.addHelpOption();
    parser.addOption({"config", "Path to the JSON configuration file", "file"});
    parser.process(app);

    QString configFilePath = parser.value("config");
    if (configFilePath.isEmpty()) {
        qWarning() << "No configuration file specified. Use --config <file> to specify the file.";
        return -1;
    }
    QJsonDocument requests;
    {
        QFile configFile(configFilePath);
        if (!configFile.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open configuration file:" << configFilePath;
            return -1;
        }
        QByteArray data = configFile.readAll();
        configFile.close();

        QJsonParseError parseError;
        requests = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "Failed to parse JSON configuration file:" << parseError.errorString();
            return -1;
        }
    }
    ShortcutManagerV3 manager;
    manager.setParent(&app);
    QObject::connect(&manager, &ShortcutManagerV3::activeChanged, &manager, [&] {
        if (!manager.isActive()) {
            return;
        }
        manager.acquire();
        for (const QJsonValue &value : requests.array()) {
            QJsonObject obj = value.toObject();
            QString req = obj.value("req").toString();
            QString name = obj.value("name").toString();
            if (req == "bind_key") {
                QString key = obj.value("key").toString();
                uint mode = static_cast<uint>(obj.value("mode").toInt());
                uint action = static_cast<uint>(obj.value("action").toInt());
                manager.bind_key(name, key, mode, action);
            } else if (req == "bind_swipe_gesture") {
                uint finger = static_cast<uint>(obj.value("finger").toInt());
                uint direction = static_cast<uint>(obj.value("direction").toInt());
                uint action = static_cast<uint>(obj.value("action").toInt());
                manager.bind_swipe_gesture(name, finger, direction, action);
            } else if (req == "bind_hold_gesture") {
                uint finger = static_cast<uint>(obj.value("finger").toInt());
                uint action = static_cast<uint>(obj.value("action").toInt());
                manager.bind_hold_gesture(name, finger, action);
            } else if (req == "unbind") {
                manager.unbind(name);
            } else {
                qWarning() << "Unknown request type:" << req;
            }
        }
    });
    app.exec();
    return 0;
}

#include "main.moc"
