// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputlayout.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputLayoutPrivate : public WObjectPrivate
{
public:
    WOutputLayoutPrivate(WOutputLayout *qq);

    void onOutputLayoutChanged();

    W_DECLARE_PUBLIC(WOutputLayout)

    QList<WOutput*> outputs;
};

WAYLIB_SERVER_END_NAMESPACE
