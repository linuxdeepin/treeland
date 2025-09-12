#pragma once

#include <wglobal.h>
#include <wsurfaceitem.h>

#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSessionLockSurface;

class WAYLIB_SERVER_EXPORT WSessionLockSurfaceItem : public WSurfaceItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SessionLockSurfaceItem)

public:
    explicit WSessionLockSurfaceItem(QQuickItem *parent = nullptr);
    ~WSessionLockSurfaceItem();

    WSessionLockSurface *sessionLockSurface() const;

private:
    Q_SLOT void onSurfaceCommit() override;
    void initSurface() override;
    QRectF getContentGeometry() const override;
};

WAYLIB_SERVER_END_NAMESPACE
