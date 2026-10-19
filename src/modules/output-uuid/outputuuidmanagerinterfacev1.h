// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QObject>

#include <memory>

WAYLIB_SERVER_USE_NAMESPACE

class OutputUuidManagerV1Private;
class OutputUuidHeadManagerV1Private;

class OutputUuidManagerV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
public:
    explicit OutputUuidManagerV1(QObject *parent = nullptr);
    ~OutputUuidManagerV1() override;

    static constexpr int InterfaceVersion = 1;

    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<OutputUuidManagerV1Private> d;
};

class OutputUuidHeadManagerV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
public:
    explicit OutputUuidHeadManagerV1(QObject *parent = nullptr);
    ~OutputUuidHeadManagerV1() override;

    static constexpr int InterfaceVersion = 1;

    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<OutputUuidHeadManagerV1Private> d;
};
