// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef TREELAND_DEBUG_HELPERS_H
#define TREELAND_DEBUG_HELPERS_H

#include <QByteArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>

// Maps a window state enum to a human-readable name.
QString stateName(int state);

// Maps a friendly button name to a Linux input button code; raw codes pass
// through. Sets *ok=false for unknown names.
int buttonCode(const QString &name, bool *ok);

// Maps a friendly key name to a Linux evdev keycode; raw codes pass through.
// Sets *ok=false for unknown names.
int keyCode(const QString &name, bool *ok);

// Writes captured image bytes to @p userPath (or a generated /tmp path when
// empty), ensuring a .png suffix. Returns the path written, or empty on
// failure.
QString saveCapture(const QByteArray &data, const QString &userPath);

// JSON helpers for geometry primitives.
QJsonObject pointToJson(const QPointF &point);
QJsonObject rectToJson(const QRectF &rect);

#endif // TREELAND_DEBUG_HELPERS_H
