// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <WServerInterface>

QW_BEGIN_NAMESPACE
class qw_security_context_manager_v1;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSecurityContextManagerPrivate;
class WAYLIB_SERVER_EXPORT WSecurityContextManager : public WObject, public WServerInterface
{
    W_DECLARE_PRIVATE(WSecurityContextManager)

public:
    WSecurityContextManager();
    ~WSecurityContextManager();
    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
