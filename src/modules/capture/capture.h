// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "impl/capturev1impl.h"
#include "itemselector.h"

#include <wglobal.h>
#include <woutput.h>
#include <wserver.h>
#include <wxdgsurface.h>
#include <wxdgsurfaceitem.h>

#include <QAbstractListModel>
#include <QPainter>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QRect>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutputRenderWindow;
class WOutputViewport;
class WToplevelSurface;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class ItemSelector;

class CaptureSource : public QObject
{
    Q_OBJECT
public:
    enum CaptureSourceType
    {
        Output = 0x1,
        Window = 0x2,
        Region = 0x4,
        Surface = 0x8,
    };
    Q_FLAG(CaptureSourceType)
    Q_DECLARE_FLAGS(CaptureSourceHint, CaptureSourceType)

    CaptureSource(WTextureProviderProvider *textureProvider, QObject *parent);

Q_SIGNALS:
    void ready();

public:
    bool valid() const;
    QImage image() const;

    /**
     * @brief DMA buffer of source, there are three cases
     * 1. output - output's dma buffer
     * 2. window - window's dma buffer
     * 3. region - output's dma buffer
     *
     * @return QW_NAMESPACE::QWBuffer*
     */
    virtual QW_NAMESPACE::qw_buffer *sourceDMABuffer() = 0;

    /**
     * @brief copyBuffer render captured contents to a buffer
     * @param buffer buffer prepared by client
     */
    void copyBuffer(QW_NAMESPACE::qw_buffer *buffer);

    // Capture area relative to the whole viewport
    virtual QRect captureRegion() = 0;

    virtual CaptureSourceType sourceType() = 0;

private:
    friend QDebug operator<<(QDebug debug, CaptureSource &captureSource);
    QImage m_image;
    WTextureProviderProvider *const m_provider;
};

#define CaptureSource_iid "org.deepin.treeland.CaptureSource"
Q_DECLARE_INTERFACE(CaptureSource, CaptureSource_iid)

class CaptureContextV1;

class CaptureContextModel : public QAbstractListModel
{
    Q_OBJECT
public:
    CaptureContextModel(QObject *parent = nullptr);

    enum CaptureContextRole
    {
        ContextRole = Qt::UserRole + 1
    };
    Q_ENUM(CaptureContextRole)
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void addContext(CaptureContextV1 *context);
    void removeContext(CaptureContextV1 *context);

private:
    QList<CaptureContextV1 *> m_captureContexts;
};

class CaptureContextV1 : public QObject
{
    Q_OBJECT
    QML_UNCREATABLE("Only created in c++")
    Q_PROPERTY(WSurface *mask READ mask NOTIFY selectInfoReady FINAL)
    Q_PROPERTY(bool freeze READ freeze NOTIFY selectInfoReady FINAL)
    Q_PROPERTY(bool withCursor READ withCursor NOTIFY selectInfoReady FINAL)
    Q_PROPERTY(CaptureSource::CaptureSourceHint sourceHint READ sourceHint NOTIFY selectInfoReady FINAL)
    Q_PROPERTY(CaptureSource *source READ source WRITE setSource NOTIFY sourceChanged FINAL)
public:
    CaptureSource *source() const;
    void setSource(CaptureSource *source);

    WSurface *mask() const;
    bool freeze() const;
    bool withCursor() const;
    CaptureSource::CaptureSourceHint sourceHint() const;

public:
    enum SourceFailure
    {
        SelectorBusy,
        Other,
    };
    Q_ENUM(SourceFailure)

    CaptureContextV1(treeland_capture_context_v1 *h, QObject *parent = nullptr);
    void sendSourceFailed(SourceFailure failure);

    inline bool hintType(CaptureSource::CaptureSourceType type)
    {
        return sourceHint().testFlag(type);
    }

Q_SIGNALS:
    void sourceChanged();
    void finishSelect();
    void selectInfoReady();

private:
    void onSelectSource();
    void onCapture(treeland_capture_frame_v1 *frame);
    void handleFrameCopy(QW_NAMESPACE::qw_buffer *buffer);

    treeland_capture_context_v1 *const m_handle;
    CaptureSource *m_captureSource;
    QPointer<treeland_capture_frame_v1> m_frame = nullptr;
};

class CaptureManagerV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
    Q_PROPERTY(CaptureContextV1 *contextInSelection READ contextInSelection NOTIFY contextInSelectionChanged FINAL)

public:
    explicit CaptureManagerV1(QObject *parent = nullptr);

    CaptureContextModel *contextModel() const
    {
        return m_captureContextModel;
    }

    CaptureContextV1 *contextInSelection() const
    {
        return m_contextInSelection;
    }

    WOutputRenderWindow *outputRenderWindow() const;
    void setOutputRenderWindow(WOutputRenderWindow *renderWindow);
    QByteArrayView interfaceName() const override;
    QPointer<WToplevelSurface> maskShellSurface() const;

Q_SIGNALS:
    void contextInSelectionChanged();
    void newCaptureContext(CaptureContextV1 *context);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private Q_SLOTS:
    void onCaptureContextSelectSource();
    void clearContextInSelection(CaptureContextV1 *context);
    void freezeAllCapturedSurface(bool freeze, WSurface *maskItem);

private:
    treeland_capture_manager_v1 *m_manager;
    CaptureContextModel *m_captureContextModel;
    CaptureContextV1 *m_contextInSelection;
    WOutputRenderWindow *m_outputRenderWindow;
    QPointF m_frozenCursorPos;
    QPointer<WToplevelSurface> m_maskShellSurface;
};

class CaptureSourceSurface : public CaptureSource
{
    Q_OBJECT
public:
    CaptureSourceSurface(WSurfaceItemContent *surfaceItemContent);
    QW_NAMESPACE::qw_buffer *sourceDMABuffer() override;
    QRect captureRegion() override;
    CaptureSourceType sourceType() override;

private:
    WSurfaceItemContent *const m_surfaceItemContent;
};

class CaptureSourceOutput : public CaptureSource
{
    Q_OBJECT
public:
    CaptureSourceOutput(WOutputViewport *viewport);
    QW_NAMESPACE::qw_buffer *sourceDMABuffer() override;
    QRect captureRegion() override;
    CaptureSourceType sourceType() override;

private:
    WOutputViewport *const m_outputViewport;
};

class CaptureSourceRegion : public CaptureSource
{
    Q_OBJECT
public:
    CaptureSourceRegion(WOutputViewport *viewport, const QRect &region);
    QW_NAMESPACE::qw_buffer *sourceDMABuffer() override;
    QRect captureRegion() override;
    CaptureSourceType sourceType() override;

private:
    WOutputViewport *const m_outputViewport;
    QRect m_region;
};

class CaptureSourceSelector : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(CaptureManagerV1* captureManager READ captureManager WRITE setCaptureManager NOTIFY captureManagerChanged REQUIRED FINAL)
    Q_PROPERTY(QRectF selectionRegion READ selectionRegion NOTIFY selectionRegionChanged FINAL)
    Q_PROPERTY(SelectionMode selectionMode READ selectionMode WRITE setSelectionMode NOTIFY selectionModeChanged FINAL)
    QML_ELEMENT

public:
    enum class SelectionMode
    {
        SelectOutput,
        SelectWindow,
        SelectRegion
    };
    Q_ENUM(SelectionMode);
    CaptureSourceSelector(QQuickItem *parent = nullptr);
    CaptureManagerV1 *captureManager() const;
    void setCaptureManager(CaptureManagerV1 *newCaptureManager);

    SelectionMode selectionMode() const;
    void setSelectionMode(const SelectionMode &newSelectionMode);
    void doSetSelectionMode(const SelectionMode &newSelectionMode);
    CaptureSource::CaptureSourceHint selectionModeHint(const SelectionMode &selectionMode);
    ItemSelector::ItemTypes selectionModeToItemTypes(const SelectionMode &selectionMode) const;

Q_SIGNALS:
    void hoveredItemChanged();
    void selectedSourceChanged();
    void captureManagerChanged();
    void selectionRegionChanged();
    void selectionModeChanged();

protected:
    void componentComplete() override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    QQuickItem *hoveredItem() const;
    QRectF selectionRegion() const;
    void setSelectionRegion(const QRectF &newSelectionRegion);
    bool itemSelectionMode() const;
    void setItemSelectionMode(bool itemSelection);
    CaptureSource *selectedSource() const;
    void setSelectedSource(CaptureSource *newSelectedSource);
    void handleItemSelectorSelectionRegionChanged();
    WOutputRenderWindow *renderWindow() const;
    void doneSelection();

    inline CaptureSource::CaptureSourceHint captureSourceHint() const
    {
        return captureManager() ? captureManager()->contextInSelection()->sourceHint()
                                : CaptureSource::CaptureSourceHint();
    }

    CaptureSource *m_selectedSource{ nullptr };
    QList<QPointer<QQuickItem>> m_selectableItems{};
    QPointer<CaptureManagerV1> m_captureManager{ nullptr };
    QRectF m_selectionRegion{};
    QPointF m_selectionAnchor{};
    bool m_itemSelectionMode{ true };
    ItemSelector *m_itemSelector{ nullptr };
    SelectionMode m_selectionMode;
    bool m_doNotFinish{ false };
};
