// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>
#include <qwoutput.h>

extern "C" {
#include <wlr/types/wlr_output_layer.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(output_layer)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(feedback, wlr_output_layer_feedback_event*)

public:
    QW_ALWAYS_INLINE static qw_output_layer *create(wlr_output *output) {
        auto layer = wlr_output_layer_create(output);
        if (!layer)
            return nullptr;
        // the wlr_output_layer will following the wlr_output to destroy,
        // so ensure the qw_output_layer following the qw_output to destroy.
        return new qw_output_layer(layer, true, qw_output::from(output));
    }

protected:
    QW_FUNC_MEMBER(output_layer, destroy, void)
};

QW_END_NAMESPACE
