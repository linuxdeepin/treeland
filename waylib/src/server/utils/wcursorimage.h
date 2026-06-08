// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QObject>
#include <QCursor>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WCursorImagePrivate;
class WAYLIB_SERVER_EXPORT WCursorImage : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QCursor cursor READ cursor WRITE setCursor NOTIFY cursorChanged FINAL)
    Q_PROPERTY(float scale READ scale WRITE setScale NOTIFY scaleChanged FINAL)
    Q_DECLARE_PRIVATE(WCursorImage)

public:
    explicit WCursorImage(QObject *parent = nullptr);

    QImage image() const;
    QPoint hotSpot() const;

    QCursor cursor() const;
    void setCursor(const QCursor &newCursor);

    float scale() const;
    void setScale(float newScale);

public Q_SLOTS:
    void setCursorTheme(const QByteArray &name, uint32_t size);

Q_SIGNALS:
    void imageChanged();
    void cursorChanged();
    void scaleChanged();

private:
    Q_PRIVATE_SLOT(d_func(), void playXCursor())
};

WAYLIB_SERVER_END_NAMESPACE
