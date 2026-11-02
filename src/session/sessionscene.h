// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "sessioncomponent.h"
#include <QList>
#include <QPointer>

class SurfaceWrapper;

// Presentation domain for every surface owned by one Session, irrespective of
// which workspace/layer/container currently holds its item.
class SessionScene : public SessionComponent
{
    Q_OBJECT
public:
    explicit SessionScene(Session *owner);
    ~SessionScene() override;
    void attach(SurfaceWrapper *surface);
protected:
    void activate() override;
    void deactivate() override;
private:
    void refresh();
    void removeDestroyed();
    bool allows(SurfaceWrapper *surface) const;
    const bool m_system;
    QList<QPointer<SurfaceWrapper>> m_surfaces;
};
