// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

struct treeland_output_manager_v2;
struct treeland_output_picture_control_v2;
WAYLIB_SERVER_USE_NAMESPACE

class Output;
class PictureControlV2Private;
class OutputManagerV2Private;

class PictureControlV2 : public QObject
{
    Q_OBJECT
public:
    ~PictureControlV2() override;

private:
    explicit PictureControlV2(wl_resource *resource, Output *output);
    friend OutputManagerV2Private;
    std::unique_ptr<PictureControlV2Private> d;
};

class OutputManagerV2
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
public:
    explicit OutputManagerV2(QObject *parent = nullptr);
    ~OutputManagerV2() override;
    static constexpr int InterfaceVersion = 1;
    static constexpr int PictureControlInterfaceVersion = 1;

    QByteArrayView interfaceName() const override;

public Q_SLOTS:
    void onPrimaryOutputChanged();

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<OutputManagerV2Private> d;
};
