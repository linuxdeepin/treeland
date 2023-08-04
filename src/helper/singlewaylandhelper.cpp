#include "singlewaylandhelper.h"

#include <QProcess>
#include <QDebug>
#include <QCoreApplication>

SingleWaylandHelper::SingleWaylandHelper(QObject *parent)
    : QObject(parent)
{}

bool SingleWaylandHelper::start(const QString &compositor, const QString &cmd)
{
    auto args = QProcess::splitCommand(cmd);

    m_process = new QProcess(this);
    m_process->setProgram(compositor);
    m_process->setArguments(args);

    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        qWarning() << m_process->readAllStandardError();
    });
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        qInfo() << m_process->readAllStandardOutput();
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            m_process, [](int exitCode, QProcess::ExitStatus exitStatus) {
        qDebug() << "wayland greeter finished" << exitCode << exitStatus;
        QCoreApplication::instance()->quit();
    });

    m_process->start();
    if (!m_process->waitForStarted(10000)) {
        qWarning("Failed to start \"%s\": %s",
                 qPrintable(cmd),
                 qPrintable(m_process->errorString()));
        return false;
    }

    return true;
}
