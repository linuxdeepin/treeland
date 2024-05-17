// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>

#include <qwglobal.h>
#include <qwkeyboard.h>

Q_MOC_INCLUDE("winputdevice.h")
Q_MOC_INCLUDE(<qwvirtualkeyboardv1.h>)

QW_BEGIN_NAMESPACE
class QWVirtualKeyboardV1;
class QWVirtualKeyboardManagerV1;
class QWKeyboard;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WQuickVirtualKeyboardV1Private;
class WQuickVirtualKeyboardManagerV1Private;
class WInputDevice;

class WQuickVirtualKeyboardV1 : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickVirtualKeyboardV1)
public:
    explicit WQuickVirtualKeyboardV1(QW_NAMESPACE::QWVirtualKeyboardV1 *handle, QObject *parent = nullptr);
    QW_NAMESPACE::QWVirtualKeyboardV1 *handle() const;
    WInputDevice *keyboard() const;
};

class WQuickVirtualKeyboardManagerV1 : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(VirtualKeyboardManagerV1)
    W_DECLARE_PRIVATE(WQuickVirtualKeyboardManagerV1)
public:
    explicit WQuickVirtualKeyboardManagerV1(QObject *parent = nullptr);

Q_SIGNALS:
    void newVirtualKeyboard(QW_NAMESPACE::QWVirtualKeyboardV1 *virtualKeyboard);

private:
    void create() override;
};
WAYLIB_SERVER_END_NAMESPACE
