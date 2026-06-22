// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpaperlauncher.h"
#include "common/treelandlogging.h"
#include "helper.h"

#include <wsocket.h>

WallpaperLauncher::WallpaperLauncher(QPointer<WSocket> socket)
    : QObject(nullptr)
    , m_socket(socket)
{
    m_launcherThread = new QThread();
    this->moveToThread(m_launcherThread);
    m_launcherThread->start();
}

WallpaperLauncher::~WallpaperLauncher()
{
    if (m_launcherThread) {
        QMetaObject::invokeMethod(this,
                                  &WallpaperLauncher::onStopRequested,
                                  Qt::BlockingQueuedConnection);
        m_launcherThread->quit();
        m_launcherThread->wait();
        m_launcherThread->deleteLater();
    }
}

void WallpaperLauncher::setDisplayName(const QString &displayName)
{
    QMetaObject::invokeMethod(this,
                              &WallpaperLauncher::onSetDisplayNameRequested,
                              Qt::QueuedConnection,
                              displayName);
}

void WallpaperLauncher::start()
{
    QMetaObject::invokeMethod(this,
                              &WallpaperLauncher::onStartRequested,
                              Qt::QueuedConnection);
}

void WallpaperLauncher::stop()
{
    QMetaObject::invokeMethod(this,
                              &WallpaperLauncher::onStopRequested,
                              Qt::QueuedConnection);
}

void WallpaperLauncher::onSetDisplayNameRequested(const QString &displayName)
{
    if (m_displayName == displayName) {
        return;
    }

    m_displayName = displayName;
}

void WallpaperLauncher::onStartRequested()
{
    if (m_wallpaperProcess) {
        qCDebug(lcTlWallpaper) << "Wallpaper already running or start requested.";
        return;
    }

    m_crashCount = 0;
    m_wallpaperProcess = new QProcess(this);
    m_wallpaperProcess->setProgram(QStringLiteral("treeland-wallpaper-factory"));
    m_wallpaperProcess->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WAYLAND_DISPLAY", m_socket->fullServerName());
    env.insert("QT_QPA_PLATFORM", "wayland");
    m_wallpaperProcess->setProcessEnvironment(env);
    connect(m_wallpaperProcess,
            &QProcess::errorOccurred,
            this,
            &WallpaperLauncher::handleWallpaperError);
    connect(m_wallpaperProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            &WallpaperLauncher::handleWallpaperFinished);
    m_wallpaperProcess->start();
}

void WallpaperLauncher::onStopRequested()
{
    if (!m_wallpaperProcess) {
        qCDebug(lcTlWallpaper) << "Wallpaper not running or stop already requested.";
        return;
    }

    Q_EMIT finished();

    if (m_wallpaperProcess->state() != QProcess::NotRunning) {
        disconnect(m_wallpaperProcess, nullptr, this, nullptr);
        m_wallpaperProcess->terminate();
        m_wallpaperProcess->waitForFinished(5000);
    }
    delete m_wallpaperProcess;
    m_wallpaperProcess = nullptr;
}

void WallpaperLauncher::handleWallpaperFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        stop();
        return;
    }

    ++m_crashCount;
    if (m_crashCount > MaxCrashCount) {
        qCCritical(lcTlWallpaper) << "Wallpaper process restart limit reached after"
                                 << m_crashCount << "abnormal exits";
        stop();
        return;
    }

    QProcessEnvironment env = m_wallpaperProcess->processEnvironment();
    env.insert("TREELAND_WALLPAPER_RESTART_COUNT", QString::number(m_crashCount));
    m_wallpaperProcess->setProcessEnvironment(env);

    qCWarning(lcTlWallpaper) << "Restarting wallpaper process after abnormal exit; exit code:"
                            << exitCode << "restart count:" << m_crashCount;
    m_wallpaperProcess->start();
}

void WallpaperLauncher::handleWallpaperError(QProcess::ProcessError error)
{
    switch (error) {
    case QProcess::FailedToStart:
        qCCritical(lcTlWallpaper) << "Wallpaper process failed to start";
        break;
    case QProcess::Crashed:
        qCCritical(lcTlWallpaper) << "Wallpaper process crashed";
        break;
    case QProcess::Timedout:
        qCCritical(lcTlWallpaper) << "Wallpaper operation timed out";
        break;
    case QProcess::WriteError:
    case QProcess::ReadError:
        qCCritical(lcTlWallpaper) << "An error occurred while communicating with Wallpaper";
        break;
    case QProcess::UnknownError:
        qCCritical(lcTlWallpaper) << "An unknown error has occurred in Wallpaper";
        break;
    }

    Q_EMIT errorOccurred();
}
