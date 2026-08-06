// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <wwaylandresource.h>
#include <wtypes.h>

#include <QObject>
#include <QRect>
#include <QQmlEngine>

Q_MOC_INCLUDE("woutput.h")
Q_MOC_INCLUDE("wsubsurface.h")

WAYLIB_SERVER_BEGIN_NAMESPACE

class WServer;
class WOutput;
class WSubsurface;
class WSurfacePrivate;
class WAYLIB_SERVER_EXPORT WSurface : public QObject, public WWaylandResource
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WSurface)
    Q_PROPERTY(bool mapped READ mapped NOTIFY mappedChanged)
    Q_PROPERTY(bool isSubsurface READ isSubsurface)
    Q_PROPERTY(bool hasSubsurface READ hasSubsurface NOTIFY hasSubsurfaceChanged)
    Q_PROPERTY(bool needsFrame READ needsFrame)
    Q_PROPERTY(QList<WSubsurface *> subsurfaces READ subsurfaces NOTIFY subsurfaceOrderChanged)
    Q_PROPERTY(uint32_t preferredBufferScale READ preferredBufferScale WRITE setPreferredBufferScale RESET resetPreferredBufferScale NOTIFY preferredBufferScaleChanged FINAL)
    QML_NAMED_ELEMENT(WaylandSurface)
    QML_UNCREATABLE("Only create in C++")

public:
    explicit WSurface(wlr_surface *handle);
    ~WSurface() override;

    wlr_surface *handle() const;

    static WSurface *fromHandle(wlr_surface *handle);

    // for current state
    bool mapped() const;
    QSize size() const;
    QSize bufferSize() const;
    WLR::Transform orientation() const;
    int bufferScale() const;
    QPoint bufferOffset() const;
    wlr_buffer *buffer() const;

    void notifyFrameDone();

    bool isSubsurface() const;
    bool hasSubsurface() const;
    const QList<WSubsurface *> &subsurfaces() const;
    QList<WSubsurface *> subsurfacesBelow() const;
    QList<WSubsurface *> subsurfacesAbove() const;

    uint32_t preferredBufferScale() const;
    void setPreferredBufferScale(uint32_t newPreferredBufferScale);
    void resetPreferredBufferScale();

    bool needsFrame() const;
    bool scheduleFrameIfNeeded();

public Q_SLOTS:
    void enterOutput(WOutput *output);
    void leaveOutput(WOutput *output);
    const QList<WOutput *> &outputs() const;
    WOutput *framePacingOutput() const;
    bool inputRegionContains(const QPointF &localPos) const;

    void map();
    void unmap();

Q_SIGNALS:
    void mappedChanged();
    void bufferOffsetChanged();
    void hasSubsurfaceChanged();
    void subsurfaceAdded(WSubsurface *subsurface);
    void subsurfaceRemoved(WSubsurface *subsurface);
    void subsurfaceOrderChanged();
    // Emitted from the destructor while the object is still usable.
    void beforeDestroy();
    void preferredBufferScaleChanged();
    void outputEntered(WOutput *output);
    void outputLeave(WOutput *output);
    void commit(quint32 committedState /*wlr_surface_state_field*/);

protected:
    WSurface(WSurfacePrivate &dd);

    // Owned by the creator (seat/shell/...): released with `delete` from
    // the native destroy callback, never with deleteLater().
    using QObject::deleteLater;

private:
    WSubsurface *addRemoteSubsurface(wlr_surface *childHandle);
    void removeSubsurface(WSubsurface *subsurface);
    void setRemoteSubsurfaceOrder(const QList<WSubsurface *> &below,
                                  const QList<WSubsurface *> &above);

    friend class WRemoteSubsurfaceManagerV1Private;
};

WAYLIB_SERVER_END_NAMESPACE
