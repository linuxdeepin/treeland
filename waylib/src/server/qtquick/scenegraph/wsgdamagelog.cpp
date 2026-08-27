// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgdamagelog_p.h"
#include "wayliblogging.h"

#include <QStringList>

namespace WSGDamageLog {

namespace {

thread_local quint64 t_damageFrame = 0;
thread_local bool t_outputOpen = false;
thread_local int t_nestedDepth = 0;
thread_local bool t_innerOpen = false;
thread_local quint64 t_frameSeq = 0;

bool damageLogEnabled()
{
    return Q_UNLIKELY(lcWlDamage().isDebugEnabled());
}

} // namespace

QString describe(const QRegion &region, bool full)
{
    if (full)
        return QStringLiteral("full");
    QStringList parts;
    for (const QRect &rect : region) {
        parts << QStringLiteral("%1x%2+%3+%4")
                     .arg(rect.width())
                     .arg(rect.height())
                     .arg(rect.x())
                     .arg(rect.y());
    }
    return parts.isEmpty() ? QStringLiteral("empty") : parts.join(QLatin1Char(','));
}

QString frameTag()
{
    if (t_damageFrame == 0)
        return QStringLiteral("[f-]");
    return QStringLiteral("[f%1]").arg(t_damageFrame);
}

void beginOutputFrame(const char *why)
{
    if (!damageLogEnabled())
        return;
    if (t_outputOpen)
        return;
    t_damageFrame = ++t_frameSeq;
    t_outputOpen = true;
    qCDebug(lcWlDamage) << "========" << frameTag()
                        << "BEGIN compositor output frame:" << why << "========";
}

void beginOutputPrepare()
{
    if (!damageLogEnabled())
        return;
    beginOutputFrame("compositor prepare (no extra scene dirty this sync)");
    qCDebug(lcWlDamage) << frameTag()
                        << "---- PREPARE begin: walk blitters, decide recapture, build scissors ----";
}

void endOutputPrepare(const QString &flush)
{
    if (!damageLogEnabled() || !t_outputOpen)
        return;
    qCDebug(lcWlDamage) << frameTag()
                        << "---- PREPARE end, flush to damage ring:" << flush << "----";
}

void beginOutputDraw()
{
    if (!damageLogEnabled() || !t_outputOpen)
        return;
    qCDebug(lcWlDamage) << frameTag()
                        << "---- DRAW begin: scene batches + blitter copy/composite ----";
}

void endOutputFrame(const char *why)
{
    if (!damageLogEnabled() || !t_outputOpen)
        return;
    qCDebug(lcWlDamage) << "========" << frameTag()
                        << "END compositor output frame:" << why << "========";
    t_outputOpen = false;
}

void beginNestedPass()
{
    if (!damageLogEnabled())
        return;
    if (++t_nestedDepth != 1)
        return;
    qCDebug(lcWlDamage) << frameTag()
                        << "---- nested MultiEffect/QSGLayer begin (not compositor output; do not recapture blitters) ----";
}

void endNestedPass()
{
    if (!damageLogEnabled())
        return;
    if (t_nestedDepth == 0)
        return;
    if (--t_nestedDepth != 0)
        return;
    qCDebug(lcWlDamage) << frameTag()
                        << "---- nested MultiEffect/QSGLayer end ----";
}

void beginInnerPass()
{
    if (!damageLogEnabled())
        return;
    if (t_innerOpen)
        return;
    t_innerOpen = true;
    qCDebug(lcWlDamage) << frameTag()
                        << "---- inner extra-QRhi begin (copy compositor pixels into this blitter; damage tracking off) ----";
}

void endInnerPass()
{
    if (!damageLogEnabled())
        return;
    if (!t_innerOpen)
        return;
    t_innerOpen = false;
    qCDebug(lcWlDamage) << frameTag()
                        << "---- inner extra-QRhi end ----";
}

} // namespace WSGDamageLog
