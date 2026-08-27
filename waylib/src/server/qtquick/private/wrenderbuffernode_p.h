// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QSGRenderNode>
#include <QPointer>
#include <QImage>
#include <QSGDynamicTexture>
#include <QMatrix4x4>
#include <QMargins>
#include <QRegion>
#include <QString>

QT_BEGIN_NAMESPACE
class QQuickItem;
class QQuickWindow;
class QSGTexture;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputRenderWindow;
class WAYLIB_SERVER_EXPORT WRenderBufferNode : public QSGRenderNode {
public:
    inline QSizeF size() const {
        return m_size;
    }
    QSGTexture *texture() const;

    static WRenderBufferNode *createRhiNode(QQuickItem *item);
    static WRenderBufferNode *createSoftwareNode(QQuickItem *item);
    // Drop window-parented blit managers while the imported GL context is still
    // alive. Call after QQuickRenderControl::invalidate() and before deleting it.
    static void destroyWindowDataManagers(QQuickWindow *window);

    QRectF rect() const override;
    RenderingFlags flags() const override;

    void resize(const QSizeF &size);
    void setContentItem(QQuickItem *item);

    typedef void(*TextureChangedNotifer)(WRenderBufferNode *node, void *data);
    void setTextureChangedCallback(TextureChangedNotifer callback, void *data);
    inline void doNotifyTextureChanged() {
        if (!m_renderCallback)
            return;
        m_renderCallback(this, m_callbackData);
    }
    virtual QImage toImage() const { return QImage(); }

    WOutputRenderWindow *renderWindow() const;
    qreal effectiveDevicePixelRatio() const;

    // QML id / objectName chain plus type, for damage logs (passwordField, …).
    QString debugLabel() const;

    // Scene transform for this frame, after WSGDamageTracker::commit().
    // `recapture` is this output's copy source (empty = reuse that
    // viewport's last capture). Each output has its own cache texture.
    void applyFrame(const QRegion &recapture, const QRect &mapped,
                    const QMatrix4x4 &sceneMatrix);
    QRect mappedQuad() const { return m_mappedRect; }
    const QMatrix4x4 &sceneMatrix() const { return m_sceneMatrix; }
    bool needsSourceCopy() const { return !m_damageRegion.isEmpty(); }
    bool hasCompositorCapture() const { return hasCompositorCaptureImpl(); }
    // Recapture is needsSourceCopy(). Draw only when that recapture is
    // pending, or the frame flush overlaps this quad (or is full).
    bool needsRender(const QRegion &flush, bool full) const;

    // Blur kernel spread, in item pixels. Copied onto the damage graph
    // GeometryNode (needsBackdrop) so flush accumulate can dilate.
    void setDamageExpansion(const QMargins &margins);
    void setDamageExpansion(int px);
    QMargins damageExpansion() const { return m_damageExpansion; }
    void setClipDamageExpansion(bool clip);
    bool clipDamageExpansion() const { return m_clipDamageExpansion; }

protected:
    WRenderBufferNode(QQuickItem *item, QSGTexture *texture);
    // RHI: false until the current output's pass has copied into that
    // viewport's texture. Attaching an empty rhiTexture in prepare() must not count.
    virtual bool hasCompositorCaptureImpl() const { return true; }

    QPointer<QQuickItem> m_item;
    QPointer<QQuickItem> m_content;
    QSizeF m_size;
    QRectF m_rect;
    QMatrix4x4 m_sceneMatrix;
    QRect m_mappedRect;
    QRegion m_damageRegion;
    QMargins m_damageExpansion;
    bool m_clipDamageExpansion = true;
    QScopedPointer<QSGTexture> m_texture;
    TextureChangedNotifer m_renderCallback = nullptr;
    void *m_callbackData = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
