// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>

#include <qwglobal.h>
#include <qwkeyboard.h>

Q_MOC_INCLUDE("winputdevice.h")

QW_BEGIN_NAMESPACE
class QWVirtualKeyboardV1;
class QWKeyboard;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WQuickVirtualKeyboardV1Private;
class WQuickVirtualKeyboardManagerV1Private;
class WInputDevice;

class WQuickVirtualKeyboardV1 : public QObject, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(VirtualKeyboardV1)
    QML_UNCREATABLE("Only created by VirtualKeyboardManagerV1 in C++")
    W_DECLARE_PRIVATE(WQuickVirtualKeyboardV1)
    Q_PROPERTY(WInputDevice* keyboard READ keyboard CONSTANT FINAL)

public:
    QW_NAMESPACE::QWVirtualKeyboardV1 *handle() const;
    WInputDevice *keyboard() const;

private:
    explicit WQuickVirtualKeyboardV1(QW_NAMESPACE::QWVirtualKeyboardV1 *handle, QObject *parent = nullptr);
    friend class WQuickVirtualKeyboardManagerV1;
};

class WQuickVirtualKeyboardManagerV1 : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(VirtualKeyboardManagerV1)
    W_DECLARE_PRIVATE(WQuickVirtualKeyboardManagerV1)
    Q_PROPERTY(QList<WQuickVirtualKeyboardV1 *> virtualKeyboards READ virtualKeyboards FINAL)

public:
    explicit WQuickVirtualKeyboardManagerV1(QObject *parent = nullptr);
    QList<WQuickVirtualKeyboardV1 *> virtualKeyboards() const;

Q_SIGNALS:
    void newVirtualKeyboard(WQuickVirtualKeyboardV1 *vk);

private:
    void create() override;
};

WAYLIB_SERVER_END_NAMESPACE
