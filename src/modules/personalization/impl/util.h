// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <tuple>

template<typename T>
struct member_function;

template<typename R, typename C, typename... Args>
struct member_function<R (C::*)(Args...)>
{
    using return_type = R;
    using class_type = C;
    using arg_typelist = std::tuple<Args...>;
};

template<typename R, typename C, typename... Args>
struct member_function<R (C::*)(Args...) const>
{
    using return_type = R;
    using class_type = C;
    using arg_typelist = std::tuple<Args...>;
};

template<std::size_t N, auto mFunc, typename typeList, typename class_type, typename... Args>
constexpr auto make_lambda()
{
    if constexpr (N == 0) {
        auto tmp = []([[maybe_unused]] struct wl_client *client, struct wl_resource *resource, Args... args) {
            auto obj = reinterpret_cast<class_type *>(class_type::fromResource(resource));
            (obj->*mFunc)(args...);
        };
        return tmp;
    } else {
        return make_lambda<N - 1,
                           mFunc,
                           typeList,
                           class_type,
                           typename std::tuple_element<N - 1, typeList>::type,
                           Args...>();
    }
}

template<auto func>
constexpr auto dispatch_member_function()
{
    using typeList = typename member_function<decltype(func)>::arg_typelist;
    using class_type = typename member_function<decltype(func)>::class_type;
    const auto typeListLen = std::tuple_size_v<typeList>;
    return make_lambda<typeListLen, func, typeList, class_type>();
}
