// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QtCore/qcompilerdetection.h>

// Private member accessor using the explicit template instantiation technique.
//
// C++ Standard [temp.explicit]/12 states:
//   "The usual access checking rules do not apply to names used to
//    specify explicit instantiation definitions."
//
// This allows passing pointers to private/protected data members and
// member functions as template arguments in explicit instantiations,
// bypassing normal access control — without modifying the class definition
// and without the UB caused by "#define private public".
//
// Usage:
//   // 1. Declare a tag and register the member pointer (at namespace scope):
//   W_DECLARE_PRIVATE_MEMBER(MyTag, SomeClass, m_secret, int)
//
//   // 2. Access the member:
//   int &val = W_PRIVATE_MEMBER(obj, MyTag{});
//   // or for a pointer:
//   int &val = W_PRIVATE_MEMBER(*ptr, MyTag{});
//
// For member functions:
//   W_DECLARE_PRIVATE_METHOD(MyFuncTag, SomeClass, privateFunc, void, int, float)
//   W_PRIVATE_CALL(obj, MyFuncTag{}, 42, 3.14f);
//
// Limitations:
//   - Does NOT work for bitfield members (C++ forbids pointer-to-member of a bitfield)
//   - Does NOT work for constructors/destructors
//   - The declaration must appear at namespace scope (not inside a function)

// GCC warns about a non-template friend inside a class template when the return
// type is template-dependent.  This is intentional: each explicit instantiation
// of W_PrivateAccessorImpl injects a unique non-template overload of get(Tag) via ADL.
// The pattern is correct; suppress the warning.
QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wnon-template-friend")

// NOTE: These helper structs must be in the SAME namespace as the Tag structs
// (global namespace, since the macros expand at file scope). If they were in a
// sub-namespace, the friend definition would create a different function than
// the friend declaration in the Tag struct, causing undefined-reference errors.

// Stores the member pointer type for a given Tag and declares the friend getter.
template<typename Tag>
struct W_PrivateAccessor {
    using MemberPtr = typename Tag::MemberPtr;
    friend constexpr MemberPtr get(Tag) noexcept;
};

// Concrete accessor — the explicit instantiation of this (with a private member
// pointer) is what legally bypasses access control.
template<typename Tag, typename Tag::MemberPtr Ptr>
struct W_PrivateAccessorImpl : W_PrivateAccessor<Tag> {
    friend constexpr typename Tag::MemberPtr get(Tag) noexcept { return Ptr; }
};

QT_WARNING_POP

// ---------------------------------------------------------------------------
// Declare access to a non-static data member.
//   TagName   – unique tag struct name (use a descriptive name)
//   ClassName – the class that owns the member
//   Member    – the private member name
//   MemberType – the type of that member
// ---------------------------------------------------------------------------
#define W_DECLARE_PRIVATE_MEMBER(TagName, ClassName, Member, MemberType) \
    struct TagName { \
        using MemberPtr = MemberType ClassName::*; \
        friend constexpr MemberPtr get(TagName) noexcept; \
    }; \
    template struct W_PrivateAccessorImpl<TagName, &ClassName::Member>

// ---------------------------------------------------------------------------
// Declare access to a non-static member function.
//   TagName    – unique tag struct name
//   ClassName  – the class that owns the method
//   MethodName – the private method name
//   RetType    – return type of the method
//   ...        – parameter types (may be empty)
// ---------------------------------------------------------------------------
#define W_DECLARE_PRIVATE_METHOD(TagName, ClassName, MethodName, RetType, ...) \
    struct TagName { \
        using MemberPtr = RetType (ClassName::*)(__VA_ARGS__); \
        friend constexpr MemberPtr get(TagName) noexcept; \
    }; \
    template struct W_PrivateAccessorImpl<TagName, &ClassName::MethodName>

// ---------------------------------------------------------------------------
// Declare access to a const non-static member function.
// ---------------------------------------------------------------------------
#define W_DECLARE_PRIVATE_CONST_METHOD(TagName, ClassName, MethodName, RetType, ...) \
    struct TagName { \
        using MemberPtr = RetType (ClassName::*)(__VA_ARGS__) const; \
        friend constexpr MemberPtr get(TagName) noexcept; \
    }; \
    template struct W_PrivateAccessorImpl<TagName, &ClassName::MethodName>

// ---------------------------------------------------------------------------
// Declare access to a static data member (pointer, not pointer-to-member).
//   TagName    – unique tag struct name
//   ClassName  – the class that owns the static member
//   Member     – the private static member name
//   MemberType – the type of that static member
// ---------------------------------------------------------------------------
#define W_DECLARE_PRIVATE_STATIC_MEMBER(TagName, ClassName, Member, MemberType) \
    struct TagName { \
        using MemberPtr = MemberType*; \
        friend constexpr MemberPtr get(TagName) noexcept; \
    }; \
    template<MemberType* Ptr> \
    struct TagName##_Impl : TagName { \
        friend constexpr MemberType* get(TagName) noexcept { return Ptr; } \
    }; \
    template struct TagName##_Impl<&ClassName::Member>

// Access a private non-static data member — returns a reference.
#define W_PRIVATE_MEMBER(obj, tag) ((obj).*get(tag))

// Call a private non-static member function.
#define W_PRIVATE_CALL(obj, tag, ...) ((obj).*get(tag))(__VA_ARGS__)

// Access a private static data member — returns a reference.
#define W_PRIVATE_STATIC_MEMBER(tag) (*get(tag))

// ---------------------------------------------------------------------------
// Bit-field storage accessor.
//
// C++ forbids forming a pointer-to-member for a bit-field, so
// W_DECLARE_PRIVATE_MEMBER cannot be used directly.  Instead, this macro
// provides memory-level access to the storage unit (the uint/unsigned that
// contains the bit fields) by computing its offset from the last non-bit-field
// private member that immediately precedes the bit-field block.
//
// Parameters:
//   TagName       – unique tag name (creates TagName and TagName_Bits helpers)
//   ClassName     – the class that owns the members
//   PrevMember    – the last non-bit-field private member before the bit fields
//   PrevMemberType – type of PrevMember
//   BfStorageType – underlying integral type of the bit-field storage unit
//                   (e.g. `uint` for `uint flag : 1;`)
//
// Bit ordering (GCC/Clang, little-endian):
//   The first-declared bit field occupies bit 0 (LSB) of the storage unit,
//   subsequent declarations occupy consecutive higher bits.
//
// Usage (at namespace scope in the .cpp file):
//   W_DECLARE_PRIVATE_BITFIELD(FooTag, Foo, m_last_regular, SomeType, uint)
//
//   // Access the preceding member directly (equivalent to W_DECLARE_PRIVATE_MEMBER):
//   SomeType &prev = W_PRIVATE_MEMBER(obj, FooTag{});
//
//   // Read bit N (0-based by declaration order):
//   bool v = W_PRIVATE_BF_GET(obj, FooTag, N);
//
//   // Write bit N:
//   W_PRIVATE_BF_SET(obj, FooTag, N, true_or_false);
// ---------------------------------------------------------------------------
#define W_DECLARE_PRIVATE_BITFIELD(TagName, ClassName, PrevMember, PrevMemberType, BfStorageType) \
    struct TagName { \
        using MemberPtr = PrevMemberType ClassName::*; \
        friend constexpr MemberPtr get(TagName) noexcept; \
    }; \
    template struct W_PrivateAccessorImpl<TagName, &ClassName::PrevMember>; \
    struct TagName##_bf_layout_mirror { PrevMemberType prev; BfStorageType storage; }; \
    struct TagName##_Bits { \
        static constexpr std::size_t kOffset = \
            __builtin_offsetof(TagName##_bf_layout_mirror, storage) - \
            __builtin_offsetof(TagName##_bf_layout_mirror, prev); \
        static BfStorageType *storagePtr(ClassName *obj) noexcept { \
            PrevMemberType &prev_ref = (*obj).*get(TagName{}); \
            return reinterpret_cast<BfStorageType*>( \
                reinterpret_cast<char *>(&prev_ref) + kOffset); \
        } \
        static const BfStorageType *storagePtr(const ClassName *obj) noexcept { \
            return storagePtr(const_cast<ClassName *>(obj)); \
        } \
        static bool getBit(const ClassName *obj, unsigned bit) noexcept { \
            return (*storagePtr(obj) >> bit) & BfStorageType{1}; \
        } \
        static void setBit(ClassName *obj, unsigned bit, bool val) noexcept { \
            BfStorageType *p = storagePtr(obj); \
            if (val) *p |= (BfStorageType{1} << bit); \
            else     *p &= ~(BfStorageType{1} << bit); \
        } \
    }

// Read bit `bit_pos` (0 = first-declared) from the bit-field storage.
#define W_PRIVATE_BF_GET(obj, TagName, bit_pos) \
    TagName##_Bits::getBit(&(obj), (bit_pos))

// Write `val` (bool) to bit `bit_pos` of the bit-field storage.
#define W_PRIVATE_BF_SET(obj, TagName, bit_pos, val) \
    TagName##_Bits::setBit(&(obj), (bit_pos), (val))
