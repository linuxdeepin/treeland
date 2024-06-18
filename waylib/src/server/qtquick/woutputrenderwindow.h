// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <woutput.h>
#include <qwglobal.h>

#include <QQuickWindow>
#include <QQmlParserStatus>

Q_MOC_INCLUDE(<wwaylandcompositor_p.h>)
Q_MOC_INCLUDE(<wquickoutputlayout.h>)

WAYLIB_SERVER_BEGIN_NAMESPACE

class WWaylandCompositor;
class WOutputViewport;
class WOutputLayer;
class WBufferRenderer;
class WOutputRenderWindowPrivate;
class WAYLIB_SERVER_EXPORT WOutputRenderWindow : public QQuickWindow, public QQmlParserStatus
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WOutputRenderWindow)
    Q_PROPERTY(WWaylandCompositor *compositor READ compositor WRITE setCompositor REQUIRED)
    Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY widthChanged)
    Q_PROPERTY(qreal height READ height WRITE setHeight NOTIFY heightChanged)
    QML_NAMED_ELEMENT(OutputRenderWindow)
    Q_INTERFACES(QQmlParserStatus)

public:
    explicit WOutputRenderWindow(QObject *parent = nullptr);
    ~WOutputRenderWindow();

    QQuickRenderControl *renderControl() const;

    void attach(WOutputViewport *output);
    void detach(WOutputViewport *output);

    void attach(WOutputLayer *layer, WOutputViewport *output);
    void detach(WOutputLayer *layer, WOutputViewport *output);

    void setOutputScale(WOutputViewport *output, float scale);
    void rotateOutput(WOutputViewport *output, WOutput::Transform t);
    void setOutputEnabled(WOutputViewport *output, bool enabled);

    WWaylandCompositor *compositor() const;
    void setCompositor(WWaylandCompositor *newRenderer);

    qreal width() const;
    qreal height() const;
    WBufferRenderer *currentRenderer() const;

public Q_SLOTS:
    void render();
    void render(WOutputViewport *output, bool doCommit);
    void scheduleRender();
    void update();
    void setWidth(qreal arg);
    void setHeight(qreal arg);

Q_SIGNALS:
    void widthChanged();
    void heightChanged();
    void outputViewportInitialized(WAYLIB_SERVER_NAMESPACE::WOutputViewport *output);

private:
    void classBegin() override;
    void componentComplete() override;

    bool event(QEvent *event) override;
};

WAYLIB_SERVER_END_NAMESPACE
