// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treelandhelper.h"

#include <WServer>
#include <WOutput>
#include <WSurfaceItem>
#include <QFile>
#include <qwcompositor.h>

#include <QRegularExpression>

extern "C" {
#define WLR_USE_UNSTABLE
#define static
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_compositor.h>
#undef static
}

TreeLandHelper::TreeLandHelper(QObject *parent)
    : Helper(parent)
{}

bool TreeLandHelper::beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event)
{
    // TODO: shortcut
    if (event->type() == QEvent::KeyPress) {
        auto e = static_cast<QKeyEvent*>(event);
        emit keyEvent(e->key(), e->modifiers());
    }

    return Helper::beforeDisposeEvent(seat, watched, event);
}

QString TreeLandHelper::socketFile() const
{
    return m_socketFile;
}

void TreeLandHelper::setSocketFile(const QString &socketFile)
{
    m_socketFile = socketFile;

    emit socketFileChanged();
}

QString TreeLandHelper::clientName(Waylib::Server::WSurface *surface) const
{
    wl_client *client = surface->handle()->handle()->resource->client;
    pid_t pid;
    uid_t uid;
    gid_t gid;
    wl_client_get_credentials(client, &pid, &uid, &gid);

    QString programName;
    QFile file(QString("/proc/%1/status").arg(pid));
    if (file.open(QFile::ReadOnly)) {
        programName = QString(file.readLine()).section(QRegularExpression("([\\t ]*:[\\t ]*|\\n)"),1,1);
    }

    qDebug() << "Program name for PID" << pid << "is" << programName;
    return programName;
}
