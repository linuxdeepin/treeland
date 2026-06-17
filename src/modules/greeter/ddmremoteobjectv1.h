// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>

#include <memory>

class DDMRemoteObjectV1Private;
class GreeterProxy;

class DDMRemoteObjectV1 : public QObject
{
    Q_OBJECT
public:
    explicit DDMRemoteObjectV1(QObject *parent = nullptr);
    ~DDMRemoteObjectV1() override;

    bool start();
    void stop();
    void setGreeterProxy(GreeterProxy *greeterProxy);
    bool isListening() const;

private:
    std::unique_ptr<DDMRemoteObjectV1Private> d;
};
