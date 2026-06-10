// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <xcb/xcb.h>

#include <QApplication>
#include <QDebug>
#include <QLabel>
#include <QMainWindow>
#include <QWidget>

int main(int argc, char **argv)
{
    // Force X11 platform
    qputenv("QT_QPA_PLATFORM", "xcb");

    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("IM Candidate Panel XProp Test");
    window.resize(400, 100);

    auto *label = new QLabel("example im-candidate-panel/xprop", &window);
    label->setAlignment(Qt::AlignCenter);
    window.setCentralWidget(label);

    // Force platform window creation before show, so we can set xprop
    WId wid = window.winId();

    // Set _DEEPIN_IM_CANDIDATE_PANEL xprop: type=CARDINAL, value=1
    xcb_connection_t *conn = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(conn)) {
        qWarning() << "Failed to connect to X server";
        return 1;
    }

    xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(conn,
                                                           0,
                                                           strlen("_DEEPIN_IM_CANDIDATE_PANEL"),
                                                           "_DEEPIN_IM_CANDIDATE_PANEL");
    xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(conn, atom_cookie, nullptr);
    if (!atom_reply) {
        qWarning() << "Failed to intern atom";
        xcb_disconnect(conn);
        return 1;
    }
    xcb_atom_t atom = atom_reply->atom;

    xcb_intern_atom_cookie_t cardinal_cookie =
        xcb_intern_atom(conn, 0, strlen("CARDINAL"), "CARDINAL");
    xcb_intern_atom_reply_t *cardinal_reply = xcb_intern_atom_reply(conn, cardinal_cookie, nullptr);
    if (!cardinal_reply) {
        qWarning() << "Failed to intern CARDINAL atom";
        free(atom_reply);
        xcb_disconnect(conn);
        return 1;
    }
    xcb_atom_t cardinal = cardinal_reply->atom;

    uint32_t value = 1;
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, wid, atom, cardinal, 32, 1, &value);
    xcb_flush(conn);

    qDebug() << "Set _DEEPIN_IM_CANDIDATE_PANEL=1 on window" << wid;

    free(cardinal_reply);
    free(atom_reply);
    xcb_disconnect(conn);

    window.show();

    return app.exec();
}
