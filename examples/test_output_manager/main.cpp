// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputuuid.h"
#include "virtualclient.h"

#include <QApplication>
#include <QCommandLineParser>

// Output management example: the GUI drives primary output selection, picture
// control (brightness / color temperature), screen configuration and virtual
// (mirrored) outputs; --uuid runs the one-shot uuid probe instead.
int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Output management example: primary output, picture control, screen configuration, "
        "virtual outputs, and the stable per-output uuid from treeland-output-uuid-unstable-v1.");
    parser.addHelpOption();

    QCommandLineOption uuidOption("uuid",
                                  "Print the stable uuid of every output through both the "
                                  "wl_output and zwlr_output_head_v1 entry points, then exit.");
    parser.addOption(uuidOption);

    QCommandLineOption timeoutOption(QStringList() << "t" << "timeout",
                                     "Milliseconds to wait for the compositor in --uuid mode.",
                                     "timeout",
                                     "2000");
    parser.addOption(timeoutOption);
    parser.process(app);

    if (parser.isSet(uuidOption)) {
        bool ok = false;
        const int timeoutMs = parser.value(timeoutOption).toInt(&ok);
        auto *probe = new UuidProbe(ok ? timeoutMs : 2000, &app);
        probe->start();
        return app.exec();
    }

    VirtualClient client;
    return app.exec();
}
