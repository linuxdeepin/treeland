// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <WServer>

#include <QObject>

Q_MOC_INCLUDE("woutput.h")
Q_MOC_INCLUDE("winputdevice.h")

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WInputDevice;
class WBackendPrivate;
class WAYLIB_SERVER_EXPORT WBackend : public QObject, public WObject,  public WServerInterface
{
    Q_OBJECT
    friend class WOutputPrivate;
    W_DECLARE_PRIVATE(WBackend)

public:
    explicit WBackend();

    wlr_backend *handle() const;
    wlr_session *session() const;

    QList<WOutput*> outputList() const;
    QList<WInputDevice*> inputDeviceList() const;

    bool hasDrm() const;
    bool hasX11() const;
    bool hasWayland() const;

    bool isSessionActive() const;

    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void outputAdded(WOutput *output);
    void outputRemoved(WOutput *output);

    void inputAdded(WInputDevice *input);
    void inputRemoved(WInputDevice *input);

    void created();

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
