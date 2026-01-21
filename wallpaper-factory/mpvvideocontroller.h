// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <mpv/client.h>

#include <QObject>
#include <QVariant>

class MpvVideoController : public QObject
{
    Q_OBJECT
public:
    explicit MpvVideoController(QObject *parent = nullptr);
    QStringView getError(int error);
    static void mpvEvents(void *ctx);
    void eventHandler();
    mpv_handle *mpv() const;

public Q_SLOTS:
    void init();
    void observeProperty(const QByteArrayView &property, mpv_format format, uint64_t id = 0);
    int unobserveProperty(uint64_t id);
    int setProperty(const QByteArrayView &property, const QVariant &value);
    int setPropertyAsync(const QByteArrayView &property, const QVariant &value, int id = 0);
    QVariant getProperty(const QByteArrayView &property);
    int getPropertyAsync(const QByteArrayView &property, int id = 0);
    QVariant command(const QVariant &params);
    int commandAsync(const QVariant &params, int id = 0);

Q_SIGNALS:
    void propertyChanged(const QByteArray &property, const QVariant &value);
    void asyncReply(const QVariant &data, mpv_event event);
    void fileStarted();
    void fileLoaded();
    void endFile(const QByteArray &reason);
    void videoReconfig();

private:
    mpv_node_list *createList(mpv_node *dst, bool is_map, int num);
    void setNode(mpv_node *dst, const QVariant &src);
    void freeNode(mpv_node *dst);
    QVariant nodeToVariant(const mpv_node *node);

private:
    mpv_handle *m_mpv = nullptr;
};
