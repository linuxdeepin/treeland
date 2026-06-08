// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wcursor.h>
Q_MOC_INCLUDE("wquickoutputlayout.h")

#include <QQuickItem>

QT_BEGIN_NAMESPACE
class QQmlComponent;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WQuickCursorAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WGlobal::CursorShape shape READ shape WRITE setShape NOTIFY shapeChanged FINAL)
    QML_ANONYMOUS

public:
    explicit WQuickCursorAttached(QQuickItem *parent);

    QQuickItem *parent() const;

    WGlobal::CursorShape shape() const;
    void setShape(WGlobal::CursorShape shape);

Q_SIGNALS:
    void shapeChanged();
};

class WQuickOutputLayout;
class WSGTextureProvider;
class WQuickCursorPrivate;
class WAYLIB_SERVER_EXPORT WQuickCursor : public QQuickItem
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WQuickCursor)
    Q_PROPERTY(bool valid READ valid NOTIFY validChanged FINAL)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WCursor* cursor READ cursor WRITE setCursor NOTIFY cursorChanged)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WOutput* output READ output WRITE setOutput NOTIFY outputChanged FINAL)
    Q_PROPERTY(QString themeName READ themeName WRITE setThemeName NOTIFY themeNameChanged)
    Q_PROPERTY(QSize sourceSize READ sourceSize WRITE setSourceSize NOTIFY sourceSizeChanged)
    Q_PROPERTY(QPointF hotSpot READ hotSpot NOTIFY hotSpotChanged FINAL)
    QML_NAMED_ELEMENT(Cursor)
    QML_ATTACHED(WQuickCursorAttached)

public:
    explicit WQuickCursor(QQuickItem *parent = nullptr);
    ~WQuickCursor();

    static WQuickCursorAttached *qmlAttachedProperties(QObject *target);

    QSGTextureProvider *textureProvider() const override;
    WSGTextureProvider *wTextureProvider() const;
    bool isTextureProvider() const override;

    bool valid() const;

    WCursor *cursor() const;
    void setCursor(WCursor *cursor);

    QString themeName() const;
    void setThemeName(const QString &name);

    QSize sourceSize() const;
    void setSourceSize(const QSize &size);

    QPointF hotSpot() const;

    WOutput *output() const;
    void setOutput(WOutput *newOutput);

Q_SIGNALS:
    void validChanged();
    void cursorChanged();
    void themeNameChanged();
    void sourceSizeChanged();
    void currentRenderWindowChanged();
    void hotSpotChanged();
    void outputChanged();

private Q_SLOTS:
    void invalidateSceneGraph();

private:
    void componentComplete() override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data) override;
    void releaseResources() override;

    W_PRIVATE_SLOT(void onImageChanged())
    W_PRIVATE_SLOT(void updateCursor())
    W_PRIVATE_SLOT(void updateImplicitSize())
    W_PRIVATE_SLOT(void updateXCursorManager())
};

WAYLIB_SERVER_END_NAMESPACE
