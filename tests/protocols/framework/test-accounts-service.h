// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <memory>

class TestAccountsService
{
public:
    TestAccountsService();
    ~TestAccountsService();

    bool registerObjects();

private:
    class Private;
    std::unique_ptr<Private> d;
};
