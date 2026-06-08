// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

template<typename T, typename M>
static inline constexpr std::ptrdiff_t offsetOf(M T::*member) noexcept
{
  static_assert(std::is_standard_layout_v<T>,
                 "T must be standard-layout");

  return static_cast<std::ptrdiff_t>(
      reinterpret_cast<std::uintptr_t>(
          &(reinterpret_cast<T const volatile*>(0)->*member)
          )
      );
}

template<typename T, typename M>
static inline constexpr T *containerOf(M *ptr, M T::*member) noexcept
{
  static_assert(std::is_standard_layout_v<T>,
                 "T must be standard-layout");

  return reinterpret_cast<T *>(
      reinterpret_cast<std::uintptr_t>(ptr) - offsetOf(member)
      );
}

