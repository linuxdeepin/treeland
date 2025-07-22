// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputlayout.h"
#include "wglobal_p.h"

#include <qwoutputlayout.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WOutputLayoutPrivate : public WWrapObjectPrivate
{
public:
    WOutputLayoutPrivate(WOutputLayout *qq);
    ~WOutputLayoutPrivate();

    WWRAP_HANDLE_FUNCTIONS(qw_output_layout, wlr_output_layout)

    void doAdd(WOutput *output);

    void instantRelease() override {
        if (handle())
            handle()->set_data(nullptr, nullptr);
    }

    W_DECLARE_PUBLIC(WOutputLayout)

    QList<WOutput*> outputs;

    void updateImplicitSize();
    int implicitWidth { 0 };
    int implicitHeight { 0 };
};

WAYLIB_SERVER_END_NAMESPACE
