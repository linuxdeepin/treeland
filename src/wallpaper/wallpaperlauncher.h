// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsocket.h"

#include <QObject>
#include <QThread>
#include <QProcess>

WAYLIB_SERVER_USE_NAMESPACE

class WallpaperLauncher : public QObject
{
    Q_OBJECT
public:
    explicit WallpaperLauncher(QPointer<WSocket> socket);
    ~WallpaperLauncher() override;

    void setDisplayName(const QString &displayName);
    void start();
    void stop();

Q_SIGNALS:
    void started(const QString &displayName);
    void finished();
    void errorOccurred();

private Q_SLOTS:
    void onSetDisplayNameRequested(const QString &displayName);
    void onStartRequested();
    void onStopRequested();

    void handleWallpaperFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleWallpaperError(QProcess::ProcessError error);

private:
    QPointer<WSocket> m_socket = nullptr;
    QThread *m_launcherThread = nullptr;
    QProcess *m_wallpaperProcess = nullptr;
    QString m_displayName;
};
