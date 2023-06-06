// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wtypes.h>

#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WOutputDamagePrivate;
class WOutputDamageHandle;
class Q_DECL_EXPORT WOutputDamage : public QObject, public WObject
{
    Q_OBJECT;
    W_DECLARE_PRIVATE(WOutputDamage);

public:
    explicit WOutputDamage(WOutput *output);

    WOutputDamageHandle *handle() const;
    template<typename DNativeInterface>
    DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }
    static WOutputDamage *fromHandle(const WOutputDamageHandle *handle);
    template<typename DNativeInterface>
    static inline WOutputDamage *fromHandle(const DNativeInterface *handle) {
        return fromHandle(reinterpret_cast<const WOutputDamageHandle*>(handle));
    }

    void add(const QRegion &region);
    void addWhole();
    void addRect(const QRect &rect);

Q_SIGNALS:
    void frame();
};

WAYLIB_SERVER_END_NAMESPACE
