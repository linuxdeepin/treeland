// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QObject>
#include <QPointer>
#include <QSharedPointer>

template<typename T>
concept IsWrapObject = std::is_base_of_v<QObject, T> && requires(T t) {
    { t.aboutToBeInvalidated() } -> std::same_as<void>;
};

template<typename T>
struct WWrapData
{
    QPointer<T> ptr;
    QMetaObject::Connection conn;

    WWrapData(T *p)
        : ptr(p)
    {
        Q_ASSERT_X(p, __func__, "WrapData is only for non-null ptr.");
        conn = QObject::connect(p,
                                &T::aboutToBeInvalidated,
                                p,
                                std::bind(&WWrapData<T>::invalidate, this));
        Q_ASSERT_X(conn, __func__, "Connection should be valid for aboutToBeInvalidated.");
    }

    void invalidate()
    {
        Q_ASSERT_X(ptr, __func__, "WrapPointer should be invalid before raw pointer destroyed.");
        ptr = nullptr;
        Q_ASSERT_X(conn, __func__, "Connection should be valid until invalidated.");
        QObject::disconnect(conn);
    }

    ~WWrapData()
    {
        if (ptr)
            invalidate();
    }
};

template<IsWrapObject T>
class WWrapPointer
{
public:
    WWrapPointer() noexcept = default;

    WWrapPointer(std::nullptr_t) noexcept
        : WWrapPointer{}
    {
    }

    WWrapPointer(T *p)
        : wd(new WWrapData<T>(p))
    {
    }

    WWrapPointer(WWrapPointer<T> &&other)
    {
        wd = std::move(other.wd);
    }

    WWrapPointer(const WWrapPointer<T> &other)
    {
        wd = other.wd;
    }

    WWrapPointer &operator=(const WWrapPointer<T> &other)
    {
        wd = other.wd;
        return *this;
    }

    WWrapPointer &operator=(WWrapPointer<T> &&other)
    {
        wd = std::move(other.wd);
        return *this;
    }

    ~WWrapPointer() { }

    WWrapPointer<T> &operator=(T *p)
    {
        wd.reset(p ? new WWrapData<T>(p) : nullptr);
        return *this;
    }

    T *data() const noexcept
    {
        return wd.data() ? wd.data()->ptr : nullptr;
    }

    T *get() const noexcept
    {
        return data();
    }

    T *operator->() const noexcept
    {
        return data();
    }

    T &operator*() const noexcept
    {
        return *data();
    }

    operator T *() const noexcept
    {
        return data();
    }

private:
    QSharedPointer<WWrapData<T>> wd;
};
