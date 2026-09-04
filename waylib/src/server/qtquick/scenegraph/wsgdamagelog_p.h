// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QRegion>
#include <QString>

// Text helpers for QT_LOGGING_RULES=waylib.renderer.damage.debug=true.
// Not a damage implementation.

namespace WSGDamageLog {

QString describe(const QRegion &region, bool full);
QString frameTag();
void beginOutputFrame(const char *why);
void beginOutputPrepare();
void endOutputPrepare(const QString &flush);
void beginOutputDraw();
void endOutputFrame(const char *why);
void beginNestedPass();
void endNestedPass();
void beginInnerPass();
void endInnerPass();

} // namespace WSGDamageLog
