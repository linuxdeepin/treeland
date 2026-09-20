// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "interfaces/windowtransition.h"

#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QRectF>

#include <optional>

class SurfaceWrapper;

//   Idle --setWindowTransitionRect--> Armed --begin--> Playing
//   Playing --endAnimation(false)--> Armed --begin--> ...
//   Playing --endAnimation(true)--> Idle   (source left, rect finished)
//   Armed/Playing --resetWindowTransition--> Idle  (source left, rect finished)
class SurfaceWindowTransition : public WindowTransitionTarget
{
    Q_OBJECT
public:
    enum class Phase
    {
        Idle,
        Armed,
        Playing,
    };

    explicit SurfaceWindowTransition(SurfaceWrapper *wrapper);

    // True when a non-empty transition rectangle is configured.
    bool active() const;
    WSurface *sourceSurface() const;
    std::optional<QRectF> globalRect() const;

    // Lifecycle driven by SurfaceWrapper.
    void begin();
    void endAnimation(bool wrapperAboutToRemove);
    void abort();

    void setWindowTransitionRect(const QRectF &localRect, WindowTransitionTarget *origin) override;
    void updateWindowTransitionRect(const QRectF &localRect) override;
    void setWindowTransitionSource(WindowTransitionSource *source) override;
    void clearWindowTransitionSource() override;
    void resetWindowTransition() override;
    std::optional<QPointF> mapTransitionPointToGlobal(const QPointF &surfacePoint) const override;

private:
    void releaseSourceOutputs();
    void onSourceSurfaceDestroyed();

    SurfaceWrapper *m_wrapper;
    Phase m_phase = Phase::Idle;
    bool m_hasRect = false;
    QRectF m_localRect;
    QPointer<WindowTransitionTarget> m_origin;
    QPointer<WindowTransitionSource> m_source;
    QList<WOutput *> m_retainedOutputs;
    QMetaObject::Connection m_sourceDestroyConnection;
};
