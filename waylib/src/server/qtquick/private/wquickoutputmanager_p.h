// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>
#include <wbackend.h>
#include <woutput.h>
#include <qwoutputmanagementv1.h>

#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

struct WOutputState
{
    Q_GADGET
    Q_PROPERTY(WOutput *output MEMBER m_output)
    Q_PROPERTY(bool enabled MEMBER m_enabled)
    Q_PROPERTY(wlr_output_mode *mode MEMBER m_mode)
    Q_PROPERTY(int32_t x MEMBER m_x)
    Q_PROPERTY(int32_t y MEMBER m_y)
    Q_PROPERTY(QSize custom_mode_size MEMBER m_custom_mode_size)
    Q_PROPERTY(int32_t custom_mode_refresh MEMBER m_custom_mode_refresh)
    Q_PROPERTY(WOutput::Transform transform MEMBER m_transform)
    Q_PROPERTY(float scale MEMBER m_scale)
    Q_PROPERTY(bool adaptive_sync_enabled MEMBER m_adaptive_sync_enabled)

public:
    WOutput *m_output;
    bool m_enabled;
    wlr_output_mode *m_mode;
    int32_t m_x, m_y;
    QSize m_custom_mode_size;
    int32_t m_custom_mode_refresh;
    WOutput::Transform m_transform;
    float m_scale;
    bool m_adaptive_sync_enabled;
};

class WQuickOutputManagerPrivate;
class WAYLIB_SERVER_EXPORT WQuickOutputManager: public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickOutputManager)
    QML_NAMED_ELEMENT(OutputManager)
public:
    explicit WQuickOutputManager(QObject *parent = nullptr);

    Q_INVOKABLE void updateConfig();
    Q_INVOKABLE QList<WOutputState> stateListPending();

    Q_INVOKABLE void sendResult(QW_NAMESPACE::QWOutputConfigurationV1 *config, bool ok);

    Q_INVOKABLE void newOutput(WOutput *output);
    Q_INVOKABLE void removeOutput(WOutput *output);

Q_SIGNALS:
    void requestTestOrApply(QW_NAMESPACE::QWOutputConfigurationV1 *config, bool onlyTest);

private:
    WServerInterface *create() override;
};

WAYLIB_SERVER_END_NAMESPACE
