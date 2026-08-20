// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "scriptrunner.h"
#include "common/treelandlogging.h"

#include <utility>
#include <algorithm>
#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>

ScriptRunner::ScriptRunner(const QString &waylandDisplay, QObject *parent)
    : QObject(parent)
    , m_waylandDisplay(waylandDisplay)
{
}

void ScriptRunner::append(const QString &dir, Type type)
{
    scanDirectory(dir, type);

    if (type == Blocking && !m_blockingRunning && !m_blockingScripts.isEmpty()) {
        m_blockingRunning = true;
        runNext(Blocking);
    }
}

void ScriptRunner::start()
{
    if (m_blockingRunning) {
        return;
    }

    if (!m_blockingScripts.isEmpty()) {
        m_blockingRunning = true;
        runNext(Blocking);
    } else {
        qCInfo(lcTlHooks) << "No blocking scripts, signaling blocking finished";
        Q_EMIT blockingFinished();
    }
}

void ScriptRunner::startNonBlocking()
{
    runNext(NonBlocking);
}

void ScriptRunner::scanDirectory(const QString &dir, Type type)
{
    QCollator collator;
    collator.setNumericMode(true);

    QDir qdir(dir);
    if (!qdir.exists()) {
        return;
    }

    auto entries = qdir.entryInfoList(QDir::Files | QDir::Executable | QDir::NoDotAndDotDot);
    std::sort(entries.begin(), entries.end(),
              [&collator](const QFileInfo &a, const QFileInfo &b) {
                  return collator(a.fileName(), b.fileName());
              });

    for (const auto &entry : std::as_const(entries)) {
        switch (type) {
        case Blocking:
            m_blockingScripts.enqueue(entry.absoluteFilePath());
            qCInfo(lcTlHooks) << "Enqueued blocking script:" << entry.absoluteFilePath();
            break;
        case NonBlocking:
            m_nonBlockingScripts.enqueue(entry.absoluteFilePath());
            qCInfo(lcTlHooks) << "Enqueued non-blocking script:" << entry.absoluteFilePath();
            break;
        }
    }
}

void ScriptRunner::runNext(Type type)
{
    auto &queue = (type == Blocking) ? m_blockingScripts : m_nonBlockingScripts;

    if (queue.isEmpty()) {
        if (type == Blocking) {
            m_blockingRunning = false;
            qCInfo(lcTlHooks) << "All blocking scripts finished";
            Q_EMIT blockingFinished();
        } else {
            qCInfo(lcTlHooks) << "All non-blocking scripts finished";
        }
        return;
    }

    const auto script = queue.dequeue();
    auto *process = createProcess(script);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, type, process](int exitCode, QProcess::ExitStatus) {
        if (exitCode != 0) {
            qCWarning(lcTlHooks) << (type == Blocking ? "Blocking" : "Non-blocking")
                                  << "script exited with code" << exitCode << ":" << process->program();
        }
        runNext(type);
    });
    connect(process, &QProcess::errorOccurred, this, [this, type, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCWarning(lcTlHooks) << (type == Blocking ? "Blocking" : "Non-blocking")
                                  << "script failed to start:" << process->program();
            runNext(type);
        }
    });
    qCInfo(lcTlHooks) << "Starting" << (type == Blocking ? "blocking" : "non-blocking")
                       << "script:" << process->program();
    process->start();
}
QProcess *ScriptRunner::createProcess(const QString &script) const
{
    auto *process = new QProcess(const_cast<ScriptRunner *>(this));
    const auto scriptName = QFileInfo(script).fileName();

    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("WAYLAND_DISPLAY"), m_waylandDisplay);
    process->setProcessEnvironment(env);
    process->setProgram(script);

    connect(process, &QProcess::readyReadStandardOutput, this, [process, scriptName]() {
        const auto data = process->readAllStandardOutput();
        for (const auto &line : QString::fromUtf8(data).split(QStringLiteral("\n"), Qt::SkipEmptyParts)) {
            qCInfo(lcTlHooks).noquote() << "[" << scriptName << "]" << line;
        }
    });
    connect(process, &QProcess::readyReadStandardError, this, [process, scriptName]() {
        const auto data = process->readAllStandardError();
        for (const auto &line : QString::fromUtf8(data).split(QStringLiteral("\n"), Qt::SkipEmptyParts)) {
            qCWarning(lcTlHooks).noquote() << "[" << scriptName << "]" << line;
        }
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            process, &QObject::deleteLater);

    return process;
}
