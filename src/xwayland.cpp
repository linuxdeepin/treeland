// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

/**
 * xwayland helper that creates an xauth file and execs Xwayland
 *
 * (dde)   treeland -> wlroots -> treeland-xwayland (this) -> Xwayland
 *            ^ |                         |
 *            | |                         +-> /tmp/.xauth_:X (dde, 0600)
 *       XwaylandName()                              |
 *            | |------------------<-----------------+
 *            | |               content
 *            | v
 * (user) treeland-sd --------> /run/user/<uid>/.xauth_XXXXXX (user, 0600)
 *                      save
 */

#include <QByteArray>
#include <QDebug>
#include <QString>
#include <QTemporaryFile>
#include <random>
#include <sys/stat.h>
#include <X11/Xauth.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        qCritical() << "Usage: treeland-xwayland <display> [args]";
        return 1;
    }
    // Generate cookie
    QByteArray cookie(16, '\0');

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 0xFF);
    for(int i = 0; i < 16; i++)
        cookie[i] = dis(gen);

    // Create xauth file
    char *display = argv[1]; // display is passed as the first argument
    const char *fileName = qPrintable(QStringLiteral("/tmp/.xauth_%1").arg(display));
    const int oldumask = umask(077);
    FILE * const authFp = fopen(fileName, "wb");
    if (!authFp)
        qFatal("fopen() failed: %s", strerror(errno));
    umask(oldumask);

    // Prepare auth entry
    Xauth auth = {};

    char localhost[HOST_NAME_MAX + 1] = "";
    if (gethostname(localhost, sizeof(localhost)) < 0)
        strcpy(localhost, "localhost");

    char cookieName[] = "MIT-MAGIC-COOKIE-1";

    // Skip the ':'
    QByteArray displayNumberUtf8 = QString(display).mid(1).toUtf8();

    auth.family = FamilyLocal;
    auth.address = localhost;
    auth.address_length = strlen(auth.address);
    auth.number = displayNumberUtf8.data();
    auth.number_length = displayNumberUtf8.size();
    auth.name = cookieName;
    auth.name_length = sizeof(cookieName) - 1;
    auth.data = cookie.data();
    auth.data_length = cookie.size();

    // Write auth
    errno = 0;
    if (XauWriteAuth(authFp, &auth) == 0)
        qFatal("XauWriteAuth(FamilyLocal) failed: %s", strerror(errno));

    // Write the same entry again, just with FamilyWild
    auth.family = FamilyWild;
    auth.address_length = 0;
    errno = 0;
    if (XauWriteAuth(authFp, &auth) == 0)
        qFatal("XauWriteAuth(FamilyWild) failed: %s", strerror(errno));

    if (fflush(authFp) != 0)
        qFatal("fflush() failed: %s", strerror(errno));

    fclose(authFp);

    // Exec Xwayland
    char **args = static_cast<char **>(malloc((argc + 3) * sizeof(char *)));
    for (int i = 0; i < argc; ++i) {
        args[i] = argv[i];
    }
    args[argc] = const_cast<char *>("-auth");
    args[argc + 1] = const_cast<char *>(fileName);
    args[argc + 2] = nullptr;
    execvp("Xwayland", args);
    qWarning() << "execvp() returned";
    return 1;
}
