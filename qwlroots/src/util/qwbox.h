// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#include <wlr/util/box.h>
}

#include <QRect>

QW_BEGIN_NAMESPACE

class qw_box
{
public:
    explicit qw_box() = default;
    explicit qw_box(const wlr_box &box):
        m_handle {.x = box.x, .y = box.y, .width = box.width, .height = box.height}
    {
    }
    explicit qw_box(const QRect &box):
        m_handle {.x = box.x(), .y = box.y(), .width = box.width(), .height = box.height()}
    {
    }

    Q_ALWAYS_INLINE QRect toQRect() {
        return QRect(m_handle.x, m_handle.y, m_handle.width, m_handle.height);
    }

    Q_ALWAYS_INLINE operator wlr_box*() {
        return &m_handle;
    }

private:
    wlr_box m_handle;
};

class qw_fbox
{
public:
    explicit qw_fbox() = default;
    explicit qw_fbox(const wlr_fbox &box):
        m_handle {.x = box.x, .y = box.y, .width = box.width, .height = box.height}
    {
    }
    explicit qw_fbox(const QRectF &box):
        m_handle {.x = box.x(), .y = box.y(), .width = box.width(), .height = box.height()}
    {
    }

    Q_ALWAYS_INLINE QRectF toQRectF() {
        return QRectF(m_handle.x, m_handle.y, m_handle.width, m_handle.height);
    }

    Q_ALWAYS_INLINE operator wlr_fbox*() {
        return &m_handle;
    }

private:
    wlr_fbox m_handle;
};

QW_END_NAMESPACE
