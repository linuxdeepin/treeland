// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "logstream.h"

#include <QThread>
#include <QTimer>

class LogStreamPrivate : public QObject
{
    Q_OBJECT
public:
    explicit LogStreamPrivate(const QString &path, QObject *parent = nullptr)
        : QObject(parent)
        , m_file(path)
        , watcher(new QFileSystemWatcher(this))
        , m_timer(new QTimer(this))
    {
        if (!m_file.open(QFile::ReadOnly | QFile::Text)) {
            qWarning() << "cannot open log file (" << path << ").";
            return;
        }

        m_timer->setSingleShot(true);
        m_timer->setInterval(500);
        m_lastPosition = m_file.size();

        watcher->addPath(path);

        connect(watcher, &QFileSystemWatcher::fileChanged, this, [this] {
            if (m_timer->isActive()) {
                return;
            }
            m_timer->start();
        });
        connect(m_timer, &QTimer::timeout, this, [this] {
            onFileChanged(m_file.fileName());
        });
    }

Q_SIGNALS:
    void bufferChanged(const QString &buffer);

private:
    void onFileChanged(const QString &path);

    qint64 m_lastPosition{ 0 };
    QFile m_file;
    QFileSystemWatcher *watcher;
    QTimer *m_timer{ nullptr };
};

void LogStreamPrivate::onFileChanged(const QString &path)
{
    if (!m_file.isOpen() && m_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Cannot reopen file");
        return;
    }

    m_file.seek(m_lastPosition);

    QTextStream stream(&m_file);
    while (!stream.atEnd()) {
        QString line = stream.readAll();
        if (!line.isNull()) {
            Q_EMIT bufferChanged(line);
        }
    }

    m_lastPosition = m_file.pos();
}

LogStream::LogStream(const QString &path, QObject *parent)
    : QObject(parent)
    , m_private(new LogStreamPrivate(path))
    , m_logThread(new QThread)
{
    m_private->moveToThread(m_logThread);
    m_logThread->start();

    connect(m_private, &LogStreamPrivate::bufferChanged, this, [this](const QString &buffer) {
        m_buffer = buffer;
        Q_EMIT bufferChanged();
    });
}

LogStream::~LogStream()
{
    m_private->deleteLater();
}

#include "logstream.moc"
