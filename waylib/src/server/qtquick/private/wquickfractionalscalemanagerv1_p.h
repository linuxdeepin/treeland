// Copyright (C) 2024 YaoBing Xiao <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>
#include <qwglobal.h>

#include <QObject>
#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickFractionalScaleManagerV1Private;
class WQuickFractionalScaleManagerV1 : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FractionalScaleManagerV1)
    W_DECLARE_PRIVATE(WQuickFractionalScaleManagerV1)

public:
    explicit WQuickFractionalScaleManagerV1(QObject *parent = nullptr);

private:
    WServerInterface *create() override;
};

WAYLIB_SERVER_END_NAMESPACE
