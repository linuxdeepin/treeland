// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <xcb/xcb.h>

#include <QObject>
#include <QVariant>

class AbstractSettings : public QObject
{
    Q_OBJECT
public:
    explicit AbstractSettings(xcb_connection_t *connection, QObject *parent = nullptr);
    ~AbstractSettings() override;

    virtual bool initialized() const = 0;
    virtual bool isEmpty() const = 0;

    virtual bool contains(const QByteArray &property) const = 0;
    virtual QVariant getPropertyValue(const QByteArray &property) const = 0;
    virtual void setPropertyValue(const QByteArray &property, const QVariant &value) = 0;
    virtual QByteArrayList propertyList() const = 0;
    virtual void apply() = 0;

Q_SIGNALS:
    void propertyChanged(const QByteArray &property, const QVariant &value);
    void propertyAdded(const QByteArray &property, const QVariant &value);
    void propertyRemoved(const QByteArray &property, const QVariant &value);

protected:
    xcb_connection_t *m_connection = nullptr;
    xcb_atom_t m_atom = XCB_ATOM_NONE;
};
