// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "cli.h"

#include "dummyddmservice.h"

#include <rep_dummyddmcontrol_replica.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QRemoteObjectNode>
#include <QRemoteObjectPendingReply>
#include <QTextStream>
#include <QUrl>

namespace {
constexpr auto defaultDdmRemoteUrl = "local:org.deepin.dde.ddm.qro.test";

bool waitBoolReply(QRemoteObjectPendingReply<bool> reply)
{
    reply.waitForFinished();
    return reply.error() == QRemoteObjectPendingCall::NoError && reply.returnValue();
}

std::unique_ptr<DummyDdmControlReplica> connectControl(const QUrl &url,
                                                       std::unique_ptr<QRemoteObjectNode> &node)
{
    node = std::make_unique<QRemoteObjectNode>();
    if (!node->connectToNode(url))
        return nullptr;

    auto replica = std::unique_ptr<DummyDdmControlReplica>(node->acquire<DummyDdmControlReplica>());
    if (!replica->waitForSource(3000))
        return nullptr;
    return replica;
}
}

int runService(QCoreApplication &app, const QStringList &args)
{
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringLiteral("ddm-url"),
                                        QStringLiteral("DDM remote url"),
                                        QStringLiteral("url"),
                                        QString::fromLatin1(defaultDdmRemoteUrl)));
    parser.addOption(QCommandLineOption(QStringLiteral("treeland-url"),
                                        QStringLiteral("Treeland remote url"),
                                        QStringLiteral("url"),
                                        QStringLiteral("local:org.deepin.dde.treeland.qro")));
    parser.addOption(QCommandLineOption(QStringLiteral("state"),
                                        QStringLiteral("State file"),
                                        QStringLiteral("path")));
    parser.addOption(QCommandLineOption(QStringLiteral("verbose"),
                                        QStringLiteral("Verbose logging")));
    parser.parse(QStringList{ QStringLiteral("treeland-ddm-debug-service") } + args);

    if (!parser.isSet(QStringLiteral("state"))) {
        qCritical() << "missing --state";
        return 2;
    }

    DummyDdmService service;
    if (!service.start(QUrl(parser.value(QStringLiteral("ddm-url"))),
                       parser.value(QStringLiteral("state")),
                       QUrl(parser.value(QStringLiteral("treeland-url"))))) {
        qCritical() << "failed to start dummy ddm service";
        return 1;
    }

    return app.exec();
}

int runCtl(QCoreApplication &app, const QStringList &args)
{
    Q_UNUSED(app)

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.addOption(QCommandLineOption(QStringLiteral("ddm-url"),
                                        QStringLiteral("DDM remote url"),
                                        QStringLiteral("url"),
                                        QString::fromLatin1(defaultDdmRemoteUrl)));
    parser.addPositionalArgument(QStringLiteral("command"), QStringLiteral("Control command"));
    parser.addPositionalArgument(QStringLiteral("args"), QStringLiteral("Command arguments"));
    parser.parse(QStringList{ QStringLiteral("treeland-ddm-debug-ctl") } + args);

    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        qCritical() << "missing control command";
        return 2;
    }

    std::unique_ptr<QRemoteObjectNode> node;
    auto control = connectControl(QUrl(parser.value(QStringLiteral("ddm-url"))), node);
    if (!control) {
        qCritical() << "failed to connect to dummy ddm control";
        return 1;
    }

    const QString command = positional.at(0);
    const QStringList commandArgs = positional.mid(1);

    if (command == QStringLiteral("lock"))
        return waitBoolReply(control->lock()) ? 0 : 1;
    if (command == QStringLiteral("unlock"))
        return waitBoolReply(control->unlock()) ? 0 : 1;
    if (command == QStringLiteral("switch-user") && commandArgs.size() == 1)
        return waitBoolReply(control->switchUser(commandArgs.at(0))) ? 0 : 1;
    if (command == QStringLiteral("set-current-user") && commandArgs.size() == 1)
        return waitBoolReply(control->setCurrentUser(commandArgs.at(0))) ? 0 : 1;
    if (command == QStringLiteral("set-last-user") && commandArgs.size() == 1)
        return waitBoolReply(control->setLastUser(commandArgs.at(0))) ? 0 : 1;
    if (command == QStringLiteral("set-last-session") && commandArgs.size() == 1)
        return waitBoolReply(control->setLastSession(commandArgs.at(0))) ? 0 : 1;
    if (command == QStringLiteral("set-remember-last-session") && commandArgs.size() == 1)
        return waitBoolReply(control->setRememberLastSession(commandArgs.at(0) == QStringLiteral("true"))) ? 0 : 1;
    if (command == QStringLiteral("set-sessions") && commandArgs.size() == 1)
        return waitBoolReply(control->setSessionsJson(commandArgs.at(0))) ? 0 : 1;
    if (command == QStringLiteral("add-user-session") && commandArgs.size() == 2)
        return waitBoolReply(control->addUserSession(commandArgs.at(0), commandArgs.at(1).toInt())) ? 0 : 1;
    if (command == QStringLiteral("remove-user-session") && commandArgs.size() == 2)
        return waitBoolReply(control->removeUserSession(commandArgs.at(0), commandArgs.at(1).toInt())) ? 0 : 1;
    if (command == QStringLiteral("set-capabilities") && commandArgs.size() == 5) {
        return waitBoolReply(control->setCapabilities(commandArgs.at(0) == QStringLiteral("true"),
                                                      commandArgs.at(1) == QStringLiteral("true"),
                                                      commandArgs.at(2) == QStringLiteral("true"),
                                                      commandArgs.at(3) == QStringLiteral("true"),
                                                      commandArgs.at(4) == QStringLiteral("true")))
            ? 0
            : 1;
    }
    if (command == QStringLiteral("emit-login-failed") && commandArgs.size() == 1)
        return waitBoolReply(control->emitLoginFailed(commandArgs.at(0))) ? 0 : 1;
    if (command == QStringLiteral("emit-info") && commandArgs.size() == 1)
        return waitBoolReply(control->emitInfo(commandArgs.at(0))) ? 0 : 1;
    if (command == QStringLiteral("status") && commandArgs.isEmpty()) {
        auto reply = control->statusJson();
        reply.waitForFinished();
        if (reply.error() != QRemoteObjectPendingCall::NoError)
            return 1;
        QTextStream(stdout) << reply.returnValue() << Qt::endl;
        return 0;
    }

    qCritical() << "unknown or invalid control command" << command << commandArgs;
    return 2;
}
