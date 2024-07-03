// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <woutputviewport.h>

#include <wserver.h>
#include <QObject>

struct treeland_virtual_output_v1;
struct treeland_virtual_output_manager_v1;
QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE


class VirtualOutputV1;
class VirtualOutputManagerAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WOutputViewport *outputViewport READ outputViewport NOTIFY outputViewportChanged FINAL)
    QML_ANONYMOUS

public:
    Q_INVOKABLE WOutputViewport *outputViewport() const { return m_viewport; };

    VirtualOutputManagerAttached(WOutputViewport *outputviewport, VirtualOutputV1 *virtualOutput);

Q_SIGNALS:

    void outputViewportChanged();
private:
    VirtualOutputV1 *m_virtualoutput;
    WOutputViewport *m_viewport;
    WOutputViewport *m_backviewport;
};

class VirtualOutputV1 : public QObject, public WServerInterface
{
    Q_OBJECT

public:
    explicit VirtualOutputV1(QObject *parent = nullptr);

    Q_INVOKABLE VirtualOutputManagerAttached *Attach(WOutputViewport *outputviewport);
    void setVirtualOutput(QString name, QStringList outputList);

    void newOutput(WOutput *output);
    void removeOutput(WOutput *output);
    QByteArrayView interfaceName() const override;

    QList<WOutputViewport *> m_viewports_list;
    QList<treeland_virtual_output_v1 *> m_virtualOutputv1;

Q_SIGNALS:

    void requestCreateVirtualOutput(QString name, QStringList outputList);
    void destroyVirtualOutput(QString name, QStringList outputList);
    void removeVirtualOutput(QStringList outputList, WOutput *output);
    void newVirtualOutput(QStringList outputList, WOutput *output);

private Q_SLOTS:
    void onVirtualOutputCreated(treeland_virtual_output_v1 *virtual_output);
    void onVirtualOutputDestroy(treeland_virtual_output_v1 *virtual_output);

private:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

    treeland_virtual_output_manager_v1 *m_manager{ nullptr };
};
