// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

struct treeland_output_manager_v1;
struct treeland_output_color_control_v1;
QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class Output;
class ColorControlV1Private;
class OutputManagerV1Private;

class ColorControlV1 : public QObject
{
    Q_OBJECT
public:
    ~ColorControlV1() override;

private:
    explicit ColorControlV1(wl_resource *resource, Output *output);
    friend OutputManagerV1Private;
    std::unique_ptr<ColorControlV1Private> d;
};

class OutputManagerV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
public:
    explicit OutputManagerV1(QObject *parent = nullptr);
    ~OutputManagerV1() override;

    QByteArrayView interfaceName() const override;

public Q_SLOTS:
    void onPrimaryOutputChanged();

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<OutputManagerV1Private> d;
};
