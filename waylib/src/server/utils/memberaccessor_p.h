// Copyright (C) 2023 - 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <utility>

namespace Waylib::PrivateAccessor
{

template<typename Tag, typename Tag::type Member>
struct Bind
{
    friend constexpr typename Tag::type waylibPrivateAccessor(Tag)
    {
        return Member;
    }
};

template<typename Tag>
constexpr typename Tag::type get()
{
    return waylibPrivateAccessor(Tag{});
}

template<typename Tag, typename Object>
decltype(auto) member(Object &&object)
{
    return std::forward<Object>(object).*get<Tag>();
}

template<typename Tag, typename Object, typename... Args>
decltype(auto) invoke(Object &&object, Args&&... args)
{
    return (std::forward<Object>(object).*get<Tag>())(std::forward<Args>(args)...);
}

template<typename Tag>
decltype(auto) staticData()
{
    return *get<Tag>();
}

template<typename Tag, typename... Args>
decltype(auto) invokeStatic(Args&&... args)
{
    return (*get<Tag>())(std::forward<Args>(args)...);
}

}

#define WAYLIB_DECLARE_PRIVATE_ACCESSOR(TAG, TYPE, VALUE) \
    struct TAG { \
        using type = TYPE; \
        friend constexpr type waylibPrivateAccessor(TAG); \
    }; \
    template struct Waylib::PrivateAccessor::Bind<TAG, VALUE>
