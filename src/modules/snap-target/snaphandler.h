// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wserver.h>

#include <QObject>

#include <memory>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutputRenderWindow;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class SnapTargetV1Private;

class SnapTargetV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
public:
    explicit SnapTargetV1(QObject *parent = nullptr);
    ~SnapTargetV1() override;

    static constexpr int InterfaceVersion = 1;

    WOutputRenderWindow *outputRenderWindow() const;
    void setOutputRenderWindow(WOutputRenderWindow *renderWindow);

    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<SnapTargetV1Private> d;
};
