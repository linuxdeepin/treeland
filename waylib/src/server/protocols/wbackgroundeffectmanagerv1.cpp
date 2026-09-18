// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wbackgroundeffectmanagerv1.h"
#include "private/wglobal_p.h"

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WBackgroundEffectManagerV1Private : public WObjectPrivate
{
public:
    WBackgroundEffectManagerV1Private(WBackgroundEffectManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    inline wlr_ext_background_effect_manager_v1 *handle() const {
        return reinterpret_cast<wlr_ext_background_effect_manager_v1*>(q_func()->m_handle);
    }

    W_DECLARE_PUBLIC(WBackgroundEffectManagerV1)
};

WBackgroundEffectManagerV1::WBackgroundEffectManagerV1()
    : WObject(*new WBackgroundEffectManagerV1Private(this))
{
}

wlr_ext_background_effect_manager_v1 *WBackgroundEffectManagerV1::handle() const
{
    return reinterpret_cast<wlr_ext_background_effect_manager_v1*>(m_handle);
}

QByteArrayView WBackgroundEffectManagerV1::interfaceName() const
{
    return "ext_background_effect_manager_v1";
}

void WBackgroundEffectManagerV1::create(WServer *server)
{
    W_D(WBackgroundEffectManagerV1);
    if (!m_handle) {
        m_handle = wlr_ext_background_effect_manager_v1_create(
            server->handle(), InterfaceVersion, 1);
    }
}

void WBackgroundEffectManagerV1::destroy(WServer *)
{
    m_handle = nullptr;
}

wl_global *WBackgroundEffectManagerV1::global() const
{
    W_D(const WBackgroundEffectManagerV1);
    if (m_handle)
        return d->handle()->global;

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
