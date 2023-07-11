// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QQuickWindow>
#include <QQmlParserStatus>

Q_MOC_INCLUDE(<wwaylandcompositor_p.h>)
Q_MOC_INCLUDE(<wquickoutputlayout.h>)

WAYLIB_SERVER_BEGIN_NAMESPACE

class WWaylandCompositor;
class WOutputViewport;
class WQuickOutputLayout;
class WOutputRenderWindowPrivate;
class WAYLIB_SERVER_EXPORT WOutputRenderWindow : public QQuickWindow, public QQmlParserStatus
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WOutputRenderWindow)
    Q_PROPERTY(WWaylandCompositor *compositor READ compositor WRITE setCompositor REQUIRED)
    Q_PROPERTY(WQuickOutputLayout* layout READ layout WRITE setLayout REQUIRED)
    QML_NAMED_ELEMENT(OutputRenderWindow)
    Q_INTERFACES(QQmlParserStatus)

public:
    enum class Layer {
        Background = -999,
        ToplevelSurface = 0,
        Cursor = 999
    };
    Q_ENUM(Layer)

    explicit WOutputRenderWindow(QObject *parent = nullptr);
    ~WOutputRenderWindow();

    QQuickRenderControl *renderControl() const;

    void attachOutput(WOutputViewport *output);
    void detachOutput(WOutputViewport *output);
    QList<WOutputViewport*> getIntersectedOutputs(const QRectF &geometry) const;
    QPointF mapToNonScaleGlobal(const QPointF &pos) const;

    WWaylandCompositor *compositor() const;
    void setCompositor(WWaylandCompositor *newRenderer);

    WQuickOutputLayout *layout() const;
    void setLayout(WQuickOutputLayout *layout);

public Q_SLOTS:
    void render();
    void scheduleRender();
    void update();

Q_SIGNALS:
    void outputLayoutChanged();

private:
    void classBegin() override;
    void componentComplete() override;

    bool event(QEvent *event) override;
};

WAYLIB_SERVER_END_NAMESPACE
