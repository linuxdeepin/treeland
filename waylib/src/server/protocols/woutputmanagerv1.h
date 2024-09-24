// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <woutput.h>

#include <WServer>

#include <QObject>

QW_BEGIN_NAMESPACE
class qw_output_manager_v1;
class qw_output_configuration_v1;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

struct WAYLIB_SERVER_EXPORT WOutputState
{
    Q_GADGET
    Q_PROPERTY(WOutput *output MEMBER output)
    Q_PROPERTY(bool enabled MEMBER enabled)
    Q_PROPERTY(wlr_output_mode *mode MEMBER mode)
    Q_PROPERTY(int32_t x MEMBER x)
    Q_PROPERTY(int32_t y MEMBER y)
    Q_PROPERTY(QSize custom_mode_size MEMBER customModeSize)
    Q_PROPERTY(int32_t custom_mode_refresh MEMBER customModeRefresh)
    Q_PROPERTY(WOutput::Transform transform MEMBER transform)
    Q_PROPERTY(float scale MEMBER scale)
    Q_PROPERTY(bool adaptive_sync_enabled MEMBER adaptiveSyncEnabled)

public:
    WOutput *output;
    bool enabled;
    wlr_output_mode *mode;
    int32_t x, y;
    QSize customModeSize;
    int32_t customModeRefresh;
    WOutput::Transform transform;
    float scale;
    bool adaptiveSyncEnabled;
};

class WOutputManagerV1Private;
class WAYLIB_SERVER_EXPORT WOutputManagerV1: public QObject, public WObject,  public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WOutputManagerV1)
public:
    explicit WOutputManagerV1();

    const QList<WOutputState> &stateListPending();

    void sendResult(QW_NAMESPACE::qw_output_configuration_v1 *config, bool ok);
    void newOutput(WOutput *output);
    void removeOutput(WOutput *output);
    QW_NAMESPACE::qw_output_manager_v1 *handle() const;
    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void requestTestOrApply(QW_NAMESPACE::qw_output_configuration_v1 *config, bool onlyTest);

protected:
    void create(WServer *server) override;
    wl_global *global() const override;

private:
    void updateConfig();
};

WAYLIB_SERVER_END_NAMESPACE
