// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QFile>
#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QTextStream>

class LogStreamPrivate;

class LogStream : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString buffer READ buffer NOTIFY bufferChanged)

public:
    explicit LogStream(const QString &path, QObject *parent = nullptr);
    ~LogStream() override;

    QString buffer() const { return m_buffer; }

Q_SIGNALS:
    void bufferChanged();

private:
    LogStreamPrivate *m_private;
    QThread *m_logThread;
    QString m_buffer;
};
