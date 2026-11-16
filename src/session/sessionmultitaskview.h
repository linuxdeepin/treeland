// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "sessioncomponent.h"
#include "interfaces/multitaskviewinterface.h"

#include <QQuickItem>

class DesktopEnvironment;
class WaylandServer;

class SessionMultitaskView : public SessionComponent, public IMultitaskView
{
    Q_OBJECT
    Q_INTERFACES(IMultitaskView)
public:
    SessionMultitaskView(Session *session, WaylandServer *runtime);
    ~SessionMultitaskView() override;

    qreal partialFactor() const override;
    void setStatus(IMultitaskView::Status status) override;
    void toggleMultitaskView(IMultitaskView::ActiveReason reason) override;
    void updatePartialFactor(qreal progress) override;
    void immediatelyExit() override;
    void commitGesture(bool triggered) override;

protected:
    void deactivate() override;

private:
    IMultitaskView *view() const;
    void onVisibleChanged();
    void reset();

    WaylandServer *const m_runtime;
    QPointer<DesktopEnvironment> m_desktop;
    QPointer<QQuickItem> m_view;
    bool m_closing = false;
};
