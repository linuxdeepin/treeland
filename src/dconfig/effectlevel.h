// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QString>

namespace EffectLevel {
enum Level : qlonglong
{
    None = 0,
    Low = 1,
    Medium = 2,
    High = 3,
};
}

constexpr bool effectEnabledForLevel(EffectLevel::Level currentLevel, qlonglong minLevel)
{
    return minLevel > EffectLevel::None && currentLevel >= minLevel;
}

inline EffectLevel::Level parseEffectLevel(const QString &value, bool *valid = nullptr)
{
    if (valid)
        *valid = true;

    if (value == QLatin1String("none"))
        return EffectLevel::None;
    if (value == QLatin1String("low"))
        return EffectLevel::Low;
    if (value == QLatin1String("medium"))
        return EffectLevel::Medium;
    if (value == QLatin1String("high"))
        return EffectLevel::High;

    if (valid)
        *valid = false;
    return EffectLevel::High;
}

inline QString effectLevelName(EffectLevel::Level level)
{
    switch (level) {
    case EffectLevel::None:
        return QStringLiteral("none");
    case EffectLevel::Low:
        return QStringLiteral("low");
    case EffectLevel::Medium:
        return QStringLiteral("medium");
    case EffectLevel::High:
        return QStringLiteral("high");
    }
    return QStringLiteral("high");
}
