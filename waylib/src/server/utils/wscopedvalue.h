// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

// Generic RAII wrapper for value-type wlroots objects that follow the
// init()/finish() (or plain zero-init + finish()) lifecycle, replacing the
// error-prone manual pattern:
//   T value; T::init(&value); ... T::finish(&value);
// which leaks the object's resources on every early-return path.
//
//   template <typename T, auto FinishFn, auto InitFn = nullptr>
// - FinishFn(T*) is invoked on destruction (required).
// - InitFn(T*) is invoked on construction; when omitted the value is simply
//   zero-initialized (e.g. wlr_drm_format, whose init is a plain memset).
template <typename T, auto FinishFn, auto InitFn = nullptr>
class WScopedValue
{
public:
    WScopedValue()
    {
        if constexpr (InitFn != nullptr)
            InitFn(&m_value);
        else
            m_value = {};
    }
    ~WScopedValue() { FinishFn(&m_value); }

    WScopedValue(const WScopedValue &) = delete;
    WScopedValue &operator=(const WScopedValue &) = delete;

    T *operator->() { return &m_value; }
    const T *operator->() const { return &m_value; }
    T *get() { return &m_value; }
    const T *get() const { return &m_value; }

private:
    T m_value {};
};

// Common specializations (add more value types as they get owned).
using WOutputStateGuard = WScopedValue<wlr_output_state, wlr_output_state_finish, wlr_output_state_init>;
using WDamageRing = WScopedValue<wlr_damage_ring, wlr_damage_ring_finish, wlr_damage_ring_init>;
using WDrmFormat = WScopedValue<wlr_drm_format, wlr_drm_format_finish>;

WAYLIB_SERVER_END_NAMESPACE
