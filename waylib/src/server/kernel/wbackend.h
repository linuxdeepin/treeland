// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>

#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WInputDevice;
class WBackendPrivate;
class WBackend : public WServerInterface, public WObject
{
    friend class WOutputPrivate;
    W_DECLARE_PRIVATE(WBackend)
public:
    WBackend();

    QVector<WOutput*> outputList() const;
    QVector<WInputDevice*> inputDeviceList() const;

protected:
    virtual void outputAdded(WOutput *output);
    virtual void outputRemoved(WOutput *output);
    virtual void inputAdded(WInputDevice *input);
    virtual void inputRemoved(WInputDevice *input);

    void create(WServer *server) override;
    void destroy(WServer *server) override;
};

WAYLIB_SERVER_END_NAMESPACE
