// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <WServer>

#include <QRegion>
#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurface;
class WBackgroundEffectManagerV1Private;
class WAYLIB_SERVER_EXPORT WBackgroundEffectManagerV1 : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WBackgroundEffectManagerV1)

public:
    explicit WBackgroundEffectManagerV1();

    wlr_ext_background_effect_manager_v1 *handle() const;

    /*
     * The committed blur region of the surface's ext-background-effect-v1
     * state, in surface-local coordinates. An empty region means the surface
     * has no background effect (or no committed blur region).
     */
    QRegion surfaceBlurRegion(WSurface *surface) const;

    QByteArrayView interfaceName() const override;
    static constexpr int InterfaceVersion = 1;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
