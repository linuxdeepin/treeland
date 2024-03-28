// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QCoreApplication>
#include <QDBusServiceWatcher>
#include <QDBusInterface>
#include <QTimer>
#include <QProcess>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  QDBusConnection::sessionBus().registerService("org.deepin.dde.Session1");

  QProcess ddeSession;
  ddeSession.start("dde-session");

  /**/
  /* QTimer::singleShot(0, &app, [] { */
  /*   QDBusInterface systemd("org.freedesktop.systemd1", */
  /*                          "/org/freedesktop/systemd1", */
  /*                          "org.freedesktop.systemd1.Manager"); */
  /*   systemd.call("UnsetEnvironment", QStringList{"DISPLAY"}); */
  /*   systemd.call("SetEnvironment", */
  /*                QStringList{ */
  /*                    QString("DISPLAY=%1").arg(qgetenv("DISPLAY")), */
  /*                }); */
  /**/
  /*   // start treeland socket */
  /*   //systemd.call("StartUnit", "treeland-sd.socket", "replace"); */
  /*   systemd.call("StartUnit", "dde-session.target", "replace"); */
  /* }); */

  return app.exec();
}
