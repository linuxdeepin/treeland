// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <woutput.h>
#include <qwglobal.h>

#include <QQuickWindow>
#include <QQmlParserStatus>

Q_MOC_INCLUDE(<wquickoutputlayout.h>)

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputViewport;
class WOutputLayer;
class WBufferRenderer;
class WOutputHelper;
class WOutputRenderWindowPrivate;
class WAYLIB_SERVER_EXPORT WOutputRenderWindow : public QQuickWindow, public QQmlParserStatus
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WOutputRenderWindow)
    Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY widthChanged)
    Q_PROPERTY(qreal height READ height WRITE setHeight NOTIFY heightChanged)
    Q_PROPERTY(bool disableLayers READ disableLayers WRITE setDisableLayers NOTIFY disableLayersChanged FINAL)
    QML_NAMED_ELEMENT(OutputRenderWindow)
    Q_INTERFACES(QQmlParserStatus)

public:
    explicit WOutputRenderWindow(QObject *parent = nullptr);
    ~WOutputRenderWindow();

    QQuickRenderControl *renderControl() const;

    void attach(WOutputViewport *output);
    void detach(WOutputViewport *output);

    void attach(WOutputLayer *layer, WOutputViewport *output);
    void attach(WOutputLayer *layer, WOutputViewport *output,
                WOutputViewport *mapFrom, QQuickItem *mapTo);
    void detach(WOutputLayer *layer, WOutputViewport *output);

    WOutputHelper *getOutputHelper(WOutputViewport *output) const;

    // TODO: Deprecate these convenience methods in favor of getOutputHelper() + setExtraState()
    // for atomic multi-property operations. These are kept for simple QML use cases.
    void setOutputScale(WOutputViewport *output, float scale);
    void rotateOutput(WOutputViewport *output, WOutput::Transform t);

    void init(QW_NAMESPACE::qw_renderer *renderer, QW_NAMESPACE::qw_allocator *allocator);
    QW_NAMESPACE::qw_renderer *renderer() const;
    QW_NAMESPACE::qw_allocator *allocator() const;

    qreal width() const;
    qreal height() const;
    WBufferRenderer *currentRenderer() const;
    bool inRendering() const;

    void setRenderEnabled(bool enabled);

    static QList<QPointer<QQuickItem>> paintOrderItemList(QQuickItem *root, std::function<bool(QQuickItem*)> filter);

    bool disableLayers() const;
    void setDisableLayers(bool newDisableLayers);

public Q_SLOTS:
    void render();
    void render(WOutputViewport *output, bool doCommit);
    void update();
    void update(WOutputViewport *output);
    void setWidth(qreal arg);
    void setHeight(qreal arg);
    void markItemClipRectDirty(QQuickItem *item);

Q_SIGNALS:
    void widthChanged();
    void heightChanged();
    void outputViewportInitialized(WAYLIB_SERVER_NAMESPACE::WOutputViewport *output);
    void initialized();
    void disableLayersChanged();
    void renderEnd(QList<QPointer<WOutput>> committedOutputs);
    void effectiveDevicePixelRatioChanged(qreal scale);

private:
    void classBegin() override;
    void componentComplete() override;

    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    friend class WOutputViewport;
    QList<WOutputLayer*> layers(const WOutputViewport *output) const;
    QList<WOutputLayer*> hardwareLayers(const WOutputViewport *output) const;
};

WAYLIB_SERVER_END_NAMESPACE
