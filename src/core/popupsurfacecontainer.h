// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "surface/surfacecontainer.h"

#include <QMouseEvent>

class PopupSurfaceContainer : public SurfaceContainer
{
    Q_OBJECT

public:
    explicit PopupSurfaceContainer(SurfaceContainer *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
};
