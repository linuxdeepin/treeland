// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QQueue>

class QProcess;

class ScriptRunner : public QObject
{
    Q_OBJECT
public:
    enum Type {
        Blocking,
        NonBlocking
    };
    Q_ENUM(Type)

    explicit ScriptRunner(const QString &waylandDisplay, QObject *parent = nullptr);

    void append(const QString &dir, Type type);

public Q_SLOTS:
    void start();
    void startNonBlocking();

Q_SIGNALS:
    void blockingFinished();

private:
    void scanDirectory(const QString &dir, Type type);
    void runNext(Type type);
    QProcess *createProcess(const QString &script) const;

    QQueue<QString> m_blockingScripts;
    QQueue<QString> m_nonBlockingScripts;
    QString m_waylandDisplay;
    bool m_blockingRunning = false;
};
