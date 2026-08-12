// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputlayout.h"
#include "wglobal_p.h"

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WOutputLayoutPrivate : public WObjectPrivate
{
public:
    WOutputLayoutPrivate(WOutputLayout *qq);
    ~WOutputLayoutPrivate();

    inline wlr_output_layout *handle() const {
        return m_handle;
    }

    void doAdd(WOutput *output);

    W_DECLARE_PUBLIC(WOutputLayout)

    QList<WOutput*> outputs;

    void updateImplicitSize();

    int implicitWidth { 0 };
    int implicitHeight { 0 };

private:
    wlr_output_layout *m_handle = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
