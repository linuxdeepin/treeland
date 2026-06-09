// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "cli.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() >= 2 && args.at(1) == QStringLiteral("service"))
        return runService(app, args.mid(2));
    if (args.size() >= 2 && args.at(1) == QStringLiteral("ctl"))
        return runCtl(app, args.mid(2));

    qCritical() << "usage: treeland-ddm-debug service|ctl ...";
    return 2;
}
