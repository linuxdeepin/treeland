// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QRemoteObjectNode>
#include <QTextStream>
#include <QUrl>

#include "rep_treeland_windowtree_replica.h"

namespace {

QJsonObject pointToJson(const QPointF &point)
{
    return {
        {"x", point.x()},
        {"y", point.y()},
    };
}

QJsonObject rectToJson(const QRectF &rect)
{
    return {
        {"x", rect.x()},
        {"y", rect.y()},
        {"width", rect.width()},
        {"height", rect.height()},
    };
}

QJsonObject windowToJson(const WindowInfo &window)
{
    return {
        {"appId", window.appId()},
        {"title", window.title()},
        {"output", window.output()},
        {"container", window.container()},
        {"workspace", window.workspace()},
        {"layer", window.layer()},
        {"z", window.z()},
        {"type", window.type()},
        {"state", window.state()},
        {"visible", window.visible()},
        {"active", window.active()},
        {"geometry", rectToJson(window.geometry())},
        {"titlebarGeometry", rectToJson(window.titlebarGeometry())},
        {"boundingRect", rectToJson(window.boundingRect())},
        {"iconGeometry", rectToJson(window.iconGeometry())},
        {"position", pointToJson(window.position())},
    };
}

QJsonArray windowsToJson(const QList<WindowInfo> &windows)
{
    QJsonArray result;
    for (const auto &window : windows)
        result.append(windowToJson(window));
    return result;
}

QJsonObject workspaceToJson(const WorkspaceInfo &workspace)
{
    return {
        {"id", workspace.id()},
        {"isActive", workspace.isActive()},
        {"windows", windowsToJson(workspace.windows())},
    };
}

QJsonArray workspacesToJson(const QList<WorkspaceInfo> &workspaces)
{
    QJsonArray result;
    for (const auto &workspace : workspaces)
        result.append(workspaceToJson(workspace));
    return result;
}

QJsonObject layerToJson(const LayerInfo &layer)
{
    return {
        {"name", layer.name()},
        {"layer", layer.layer()},
        {"windows", windowsToJson(layer.windows())},
        {"workspaces", workspacesToJson(layer.workspaces())},
    };
}

QJsonArray layersToJson(const QList<LayerInfo> &layers)
{
    QJsonArray result;
    for (const auto &layer : layers)
        result.append(layerToJson(layer));
    return result;
}

QJsonObject treelandInfoToJson(const TreelandInfo &info)
{
    return {
        {"currentMode", info.currentMode()},
        {"layers", layersToJson(info.layers())},
    };
}

void registerNamedMetatypes()
{
    WindowTreeRemoteReplica::registerMetatypes();
    qRegisterMetaType<WindowInfo>("WindowInfo");
    qRegisterMetaType<QList<WindowInfo>>("QList<WindowInfo>");
    qRegisterMetaType<WorkspaceInfo>("WorkspaceInfo");
    qRegisterMetaType<QList<WorkspaceInfo>>("QList<WorkspaceInfo>");
    qRegisterMetaType<LayerInfo>("LayerInfo");
    qRegisterMetaType<QList<LayerInfo>>("QList<LayerInfo>");
    qRegisterMetaType<TreelandInfo>("TreelandInfo");
}

int fail(const QString &message)
{
    QTextStream(stderr) << "treeland-debug: " << message << Qt::endl;
    return EXIT_FAILURE;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("Inspect Treeland debug Remote Objects; currently supports WindowTree.");
    parser.addHelpOption();

    const QCommandLineOption urlOption(
        "url", "Qt Remote Objects host URL.", "url", "local:org.deepin.dde.treeland.debug");
    const QCommandLineOption nameOption("name", "Remote Object name.", "name", "WindowTree");
    const QCommandLineOption timeoutOption("timeout-ms", "Request timeout in milliseconds.", "milliseconds", "30000");
    const QCommandLineOption treeOption("tree", "Print the complete window tree.");
    const QCommandLineOption cursorOption("cursor", "Print the cursor position instead of the window tree.");
    parser.addOption(urlOption);
    parser.addOption(nameOption);
    parser.addOption(timeoutOption);
    parser.addOption(treeOption);
    parser.addOption(cursorOption);
    parser.process(application);

    if (parser.isSet(treeOption) && parser.isSet(cursorOption))
        return fail("--tree and --cursor cannot be used together");

    bool timeoutValid = false;
    const int timeoutMs = parser.value(timeoutOption).toInt(&timeoutValid);
    if (!timeoutValid || timeoutMs < 0)
        return fail("--timeout-ms must be a non-negative integer");

    registerNamedMetatypes();

    const QString url = parser.value(urlOption);
    QRemoteObjectNode node;
    if (!node.connectToNode(QUrl(url)))
        return fail(QStringLiteral("failed to connect to remote object node: %1").arg(url));

    const QString name = parser.value(nameOption);
    auto *replica = node.acquire<WindowTreeRemoteReplica>(name);
    if (!replica->waitForSource(timeoutMs))
        return fail(QStringLiteral("timed out waiting for WindowTreeRemote source: %1").arg(name));

    QJsonDocument document;
    if (parser.isSet(cursorOption)) {
        document = QJsonDocument(pointToJson(replica->cursorPosition()));
    } else {
        auto reply = replica->getTreelandInfo();
        if (!reply.waitForFinished(timeoutMs))
            return fail("timed out waiting for getTreelandInfo()");
        if (reply.error() != QRemoteObjectPendingCall::NoError)
            return fail("getTreelandInfo() returned a Qt Remote Objects error");
        document = QJsonDocument(treelandInfoToJson(reply.returnValue()));
    }

    QTextStream(stdout) << QString::fromUtf8(document.toJson(QJsonDocument::Indented));
    return EXIT_SUCCESS;
}
