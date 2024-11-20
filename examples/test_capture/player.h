// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <private/qsgplaintexture_p.h>

#include <QQuickItem>
class QSGPlainTexture;
class QRhiTexture;
class TreelandCaptureContext;

class Player : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(TreelandCaptureContext* captureContext READ captureContext WRITE setCaptureContext NOTIFY captureContextChanged FINAL)
public:
    Player();

    TreelandCaptureContext *captureContext() const;
    void setCaptureContext(TreelandCaptureContext *context);

Q_SIGNALS:
    void captureContextChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;

private:
    void updateTexture();
    void updateGeometry();
    void ensureDebugLogger();

    QSGPlainTexture m_texture;
    QRhiTexture *m_rhiTexture{ nullptr };
    QPointer<TreelandCaptureContext> m_captureContext{};
    bool m_loggerInitialized{ false };
};
