// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QColor>
#include <QPoint>

/**
 * @brief Drop-shadow parameters for server-side window decoration.
 *
 * Shared between the deprecated treeland-personalization-manager-v1
 * protocol and the new treeland-decoration-unstable-v1 protocol so that
 * both can describe the same physical concept without code duplication.
 */
struct Shadow
{
    int32_t radius;
    QPoint offset;
    QColor color;
    bool operator==(const Shadow &other) const = default;
};

/**
 * @brief Border parameters for server-side window decoration.
 *
 * Shared between the deprecated treeland-personalization-manager-v1
 * protocol and the new treeland-decoration-unstable-v1 protocol.
 */
struct Border
{
    int32_t width;
    QColor color;
    bool operator==(const Border &other) const = default;
};
