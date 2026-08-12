// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Negative compile check: this file MUST NOT build. container_of() exists to
// reject non-standard-layout container types (a member pointer or raw offset
// into such an object is not guaranteed to be usable), enforced by
// static_assert(std::is_standard_layout_v<T>). CMakeLists.txt tries to
// compile this translation unit and fails the configuration if it succeeds.

#include <wcontainerof.h>

struct NonStandardLayout {
    virtual ~NonStandardLayout() = default;   // virtual: not standard-layout
    int member;
};

int main()
{
    NonStandardLayout obj;
    auto *p = W_CONTAINER_OF(&obj.member, NonStandardLayout, member);
    (void)p;
    return 0;
}
