// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "debugswitch.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

#include <algorithm>
#include <pwd.h>
#include <sys/stat.h>

QList<uid_t> treelandCompositorUids()
{
    QList<uid_t> uids;
    const QDir proc(QStringLiteral("/proc"));
    const QStringList entries = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool isPid = false;
        entry.toUInt(&isPid);
        if (!isPid)
            continue;
        const QString dir = proc.filePath(entry);
        QFile comm(dir + QStringLiteral("/comm"));
        if (!comm.open(QIODevice::ReadOnly) || comm.readAll().trimmed() != QByteArrayLiteral("treeland"))
            continue;
        struct stat st {};
        if (::stat(QFile::encodeName(dir).constData(), &st) != 0)
            continue;
        const uid_t uid = static_cast<uid_t>(st.st_uid);
        if (!uids.contains(uid))
            uids.append(uid);
    }
    std::sort(uids.begin(), uids.end());
    return uids;
}

QString treelandUserName(uid_t uid)
{
    if (const struct passwd *pw = ::getpwuid(uid))
        return QString::fromLocal8Bit(pw->pw_name);
    return QString::number(uid);
}

bool treelandDebugTargetUid(uid_t *uid, QString *error)
{
    // A session instance normally runs as whoever asks and may share the machine
    // with a global service, so our own uid wins when a compositor has it.
    const QList<uid_t> uids = treelandCompositorUids();
    const uid_t self = ::geteuid();
    if (uids.contains(self)) {
        *uid = self;
        return true;
    }
    if (uids.size() == 1) {
        *uid = uids.first();
        return true;
    }
    if (uids.size() > 1) {
        QStringList names;
        for (const uid_t candidate : uids)
            names.append(QStringLiteral("%1 (uid %2)").arg(treelandUserName(candidate)).arg(candidate));
        *error = QStringLiteral("several compositors are running, as %1; the switch is "
                                "per user, so write the one you mean with `dde-dconfig -u "
                                "<uid> set -a org.deepin.dde.treeland -r "
                                "org.deepin.dde.treeland -k remoteDebug -v true|false`")
                     .arg(names.join(QStringLiteral(", ")));
        return false;
    }
    *uid = self;
    return true;
}

namespace {

// Runs `dde-dconfig`, the DTK CLI for the config manager on the system bus.
// `-u <uid>` names the user whose app-scoped keys are meant, which is how root
// writes another user's switch.  Returns false only when the process could not
// be run at all: the tool reports refusals as text and still exits 0, so every
// caller judges the result by the value it reads back.
bool runDConfig(const QStringList &args, int timeoutMs, QString *out, QString *err)
{
    const QString exe = QStandardPaths::findExecutable(QStringLiteral("dde-dconfig"));
    if (exe.isEmpty()) {
        *err = QStringLiteral("dde-dconfig was not found — install the dde-dconfig package");
        return false;
    }
    QProcess process;
    process.start(exe, args);
    if (!process.waitForStarted(qMin(5000, timeoutMs))) {
        *err = process.errorString();
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished();
        *err = QStringLiteral("dde-dconfig did not answer");
        return false;
    }
    *out = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    *err = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    return true;
}

QStringList remoteDebugArgs(uid_t uid, const QString &action)
{
    return { QStringLiteral("-u"),
             QString::number(uid),
             action,
             QStringLiteral("-a"),
             QStringLiteral("org.deepin.dde.treeland"),
             QStringLiteral("-r"),
             QStringLiteral("org.deepin.dde.treeland"),
             QStringLiteral("-k"),
             QStringLiteral("remoteDebug") };
}

QString dconfigError(const QString &what, const QString &out, const QString &err)
{
    const QString detail = (out + QLatin1Char(' ') + err).trimmed();
    return detail.isEmpty() ? what : what + QStringLiteral(": ") + detail;
}

} // namespace

bool treelandRemoteDebugSwitch(uid_t uid, int want, bool *enabled, QString *error,
                               int timeoutMs)
{
    QString out;
    QString err;
    const QString wanted = want == 1 ? QStringLiteral("true") : QStringLiteral("false");
    if (want >= 0) {
        QStringList setArgs = remoteDebugArgs(uid, QStringLiteral("set"));
        setArgs.append({ QStringLiteral("-v"), wanted });
        if (!runDConfig(setArgs, timeoutMs, &out, &err)) {
            *error = dconfigError(QStringLiteral("cannot run dde-dconfig"), out, err);
            return false;
        }
    }
    if (!runDConfig(remoteDebugArgs(uid, QStringLiteral("get")), timeoutMs, &out, &err)) {
        *error = dconfigError(QStringLiteral("cannot run dde-dconfig"), out, err);
        return false;
    }
    if (out != QLatin1String("true") && out != QLatin1String("false")) {
        *error = dconfigError(QStringLiteral("remoteDebug is unreadable"), out, err);
        return false;
    }
    if (want >= 0 && out != wanted) {
        *error = dconfigError(QStringLiteral("the switch did not change"), out, err);
        return false;
    }
    *enabled = out == QLatin1String("true");
    return true;
}
