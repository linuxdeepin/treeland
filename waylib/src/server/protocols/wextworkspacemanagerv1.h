// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <wglobal.h>
#include <QObject>
#include <QList>
#include <QString>
#include <cstdint>

Q_MOC_INCLUDE("wlr_all.h")

struct wlr_ext_workspace_manager_v1;
struct wlr_ext_workspace_group_handle_v1;
struct wlr_ext_workspace_handle_v1;
struct wlr_ext_workspace_v1_commit_event;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;

class WAYLIB_SERVER_EXPORT WExtWorkspaceManagerV1
    : public QObject, public WServerInterface, public WObject
{
    Q_OBJECT
public:
    explicit WExtWorkspaceManagerV1(QObject *parent = nullptr);
    ~WExtWorkspaceManagerV1() override;
    QByteArrayView interfaceName() const override;
    wlr_ext_workspace_manager_v1 *handle() const;
    static constexpr int InterfaceVersion = 1;

    wlr_ext_workspace_group_handle_v1 *createGroup(uint32_t capabilities);
    void removeGroup(wlr_ext_workspace_group_handle_v1 *group);
    void outputEnter(wlr_ext_workspace_group_handle_v1 *group, WOutput *output);
    void outputLeave(wlr_ext_workspace_group_handle_v1 *group, WOutput *output);
    wlr_ext_workspace_handle_v1 *createWorkspace(const QByteArray &id, uint32_t capabilities);
    void removeWorkspace(wlr_ext_workspace_handle_v1 *workspace);
    void setGroup(wlr_ext_workspace_handle_v1 *workspace, wlr_ext_workspace_group_handle_v1 *group);
    void setName(wlr_ext_workspace_handle_v1 *workspace, const QString &name);
    void setCoordinates(wlr_ext_workspace_handle_v1 *workspace, const QList<uint32_t> &coordinates);
    void setState(wlr_ext_workspace_handle_v1 *workspace, bool active, bool urgent, bool hidden);

Q_SIGNALS:
    void initialized();
    void invalidated();
    // The request list is owned by wlroots and valid only during this signal.
    // Consumers must use a direct connection and process requests in list order.
    void commit(wlr_ext_workspace_v1_commit_event *event);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    void onCommit(wlr_ext_workspace_v1_commit_event *event);
    void onNativeDestroyed();
};
WAYLIB_SERVER_END_NAMESPACE
