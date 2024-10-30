// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QColor>
#include <QPoint>

struct Shadow
{
    int32_t radius;
    QPoint offset;
    QColor color;
};

struct Border
{
    int32_t width;
    QColor color;
};

enum ThemeType
{
    Auto = 0,
    Light = 1,
    Dark = 2,
};

