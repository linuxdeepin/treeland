// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>

#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WCursor;

class WQuickCursorShapeManagerPrivate;
class WAYLIB_SERVER_EXPORT WQuickCursorShapeManager: public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickCursorShapeManager)
    QML_NAMED_ELEMENT(CursorShapeManager)

public:
    explicit WQuickCursorShapeManager(QObject *parent = nullptr);

private:
    void create() override;
};

WAYLIB_SERVER_END_NAMESPACE
