// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <WServer>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WSurface;

class WAYLIB_SERVER_EXPORT WPresentation : public QObject, public WServerInterface
{
    Q_OBJECT

public:
    explicit WPresentation(wlr_backend *backend, QObject *parent = nullptr);

    wlr_presentation *handle() const;

    QByteArrayView interfaceName() const override;

    void surfaceTexturedOnOutput(WSurface *surface, WOutput *output);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    wlr_backend *m_backend = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
