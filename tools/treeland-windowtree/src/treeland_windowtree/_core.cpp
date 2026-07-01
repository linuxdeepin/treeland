// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <pybind11/pybind11.h>

#include <QCoreApplication>
#include <QDebug>
#include <QPointF>
#include <QRectF>
#include <QRemoteObjectNode>
#include <QUrl>

#include <memory>
#include <stdexcept>
#include <string>

#include "rep_treeland_windowtree_replica.h"

namespace py = pybind11;

namespace {

QCoreApplication *owned_app = nullptr;

void ensure_qt_application()
{
    if (QCoreApplication::instance())
        return;

    static int argc = 1;
    static char app_name[] = "treeland-windowtree";
    static char *argv[] = {app_name, nullptr};
    // Keep the Qt application alive until process exit. Destroying a
    // QCoreApplication from a Python extension during interpreter shutdown can
    // run after Qt globals have already been torn down on some systems.
    owned_app = new QCoreApplication(argc, argv);
}

std::string to_string(const QString &value)
{
    return value.toStdString();
}

py::dict point_to_dict(const QPointF &point)
{
    py::dict result;
    result["x"] = point.x();
    result["y"] = point.y();
    return result;
}

py::dict rect_to_dict(const QRectF &rect)
{
    py::dict result;
    result["x"] = rect.x();
    result["y"] = rect.y();
    result["width"] = rect.width();
    result["height"] = rect.height();
    return result;
}

py::dict window_to_dict(const WindowInfo &window)
{
    py::dict result;
    result["appId"] = to_string(window.appId());
    result["title"] = to_string(window.title());
    result["output"] = to_string(window.output());
    result["container"] = to_string(window.container());
    result["workspace"] = window.workspace();
    result["layer"] = window.layer();
    result["z"] = window.z();
    result["type"] = window.type();
    result["state"] = window.state();
    result["visible"] = window.visible();
    result["active"] = window.active();
    result["geometry"] = rect_to_dict(window.geometry());
    result["titlebarGeometry"] = rect_to_dict(window.titlebarGeometry());
    result["boundingRect"] = rect_to_dict(window.boundingRect());
    result["iconGeometry"] = rect_to_dict(window.iconGeometry());
    result["position"] = point_to_dict(window.position());
    return result;
}

py::list windows_to_list(const QList<WindowInfo> &windows)
{
    py::list result;
    for (const auto &window : windows)
        result.append(window_to_dict(window));
    return result;
}

py::dict workspace_to_dict(const WorkspaceInfo &workspace)
{
    py::dict result;
    result["id"] = workspace.id();
    result["isActive"] = workspace.isActive();
    result["windows"] = windows_to_list(workspace.windows());
    return result;
}

py::list workspaces_to_list(const QList<WorkspaceInfo> &workspaces)
{
    py::list result;
    for (const auto &workspace : workspaces)
        result.append(workspace_to_dict(workspace));
    return result;
}

py::dict layer_to_dict(const LayerInfo &layer)
{
    py::dict result;
    result["name"] = to_string(layer.name());
    result["layer"] = layer.layer();
    result["windows"] = windows_to_list(layer.windows());
    result["workspaces"] = workspaces_to_list(layer.workspaces());
    return result;
}

py::list layers_to_list(const QList<LayerInfo> &layers)
{
    py::list result;
    for (const auto &layer : layers)
        result.append(layer_to_dict(layer));
    return result;
}

py::dict treelandinfo_to_dict(const TreelandInfo &info)
{
    py::dict result;
    result["currentMode"] = to_string(info.currentMode());
    result["layers"] = layers_to_list(info.layers());
    return result;
}

void register_named_metatypes()
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

} // namespace

class WindowTreeClient
{
public:
    WindowTreeClient(
        const std::string &url = "local:org.deepin.dde.treeland.debug",
        const std::string &name = "WindowTree",
        int timeout_ms = 30000)
        : timeout_ms_(timeout_ms)
    {
        ensure_qt_application();
        register_named_metatypes();

        node_ = std::make_unique<QRemoteObjectNode>();
        if (!node_->connectToNode(QUrl(QString::fromStdString(url)))) {
            throw std::runtime_error("failed to connect to remote object node: " + url);
        }

        replica_ = node_->acquire<WindowTreeRemoteReplica>(QString::fromStdString(name));
        bool ready = false;
        {
            py::gil_scoped_release release;
            ready = replica_->waitForSource(timeout_ms_);
        }
        if (!ready) {
            throw std::runtime_error("timed out waiting for WindowTreeRemote source: " + name);
        }
    }

    py::dict get_full_layout_tree()
    {
        auto reply = replica_->getTreelandInfo();
        bool finished = false;
        {
            py::gil_scoped_release release;
            finished = reply.waitForFinished(timeout_ms_);
        }

        if (!finished)
            throw std::runtime_error("timed out waiting for getTreelandInfo()");
        if (reply.error() != QRemoteObjectPendingCall::NoError)
            throw std::runtime_error("getTreelandInfo() returned a Qt Remote Objects error");

        return treelandinfo_to_dict(reply.returnValue());
    }

    py::dict cursor_position() const
    {
        return point_to_dict(replica_->cursorPosition());
    }

private:
    int timeout_ms_;
    std::unique_ptr<QRemoteObjectNode> node_;
    WindowTreeRemoteReplica *replica_ = nullptr;
};

PYBIND11_MODULE(_core, m)
{
    py::class_<WindowTreeClient>(m, "WindowTreeClient")
        .def(
            py::init<const std::string &, const std::string &, int>(),
            py::arg("url") = "local:org.deepin.dde.treeland.debug",
            py::arg("name") = "WindowTree",
            py::arg("timeout_ms") = 30000)
        .def("get_full_layout_tree", &WindowTreeClient::get_full_layout_tree)
        .def("cursor_position", &WindowTreeClient::cursor_position);

    m.def(
        "get_full_layout_tree",
        [](const std::string &url, const std::string &name, int timeout_ms) {
            WindowTreeClient client(url, name, timeout_ms);
            return client.get_full_layout_tree();
        },
        py::arg("url") = "local:org.deepin.dde.treeland.debug",
        py::arg("name") = "WindowTree",
        py::arg("timeout_ms") = 30000);

    m.def(
        "get_cursor_position",
        [](const std::string &url, const std::string &name, int timeout_ms) {
            WindowTreeClient client(url, name, timeout_ms);
            return client.cursor_position();
        },
        py::arg("url") = "local:org.deepin.dde.treeland.debug",
        py::arg("name") = "WindowTree",
        py::arg("timeout_ms") = 30000);
}
