#pragma once

#include <variant>

#include "ObjHandle.h"
#include "Types.h"

enum class ValueType : u8 { Bool = 1, F64 = 2, U64 = 3, Nil = 4, Obj = 5 };

class Value
{
public:
    Value();
    Value(bool val);
    Value(f64 val);
    Value(u64 val);
    Value(void* val);
    Value(ObjHandle val);
    ValueType GetType() const;
    bool HasType(ValueType type) const;
    template <typename T>
    bool HasType() const;
    template <typename T>
    const T& As() const;
    template <typename T>
    T& As();
    template <typename T>
    const T* Try() const;
    template <typename T>
    T* Try() const;
private:
    union Val
    {
        bool Bool;
        f64 F64;
        u64 U64;
        void* Nil;
        ObjHandle Obj;
    };
    Val m_Val{.Nil = nullptr };
    ValueType m_Type{ValueType::Nil};
};

template <typename T>
bool Value::HasType() const
{
    static_assert(
        std::is_same_v<T, bool> ||
        std::is_same_v<T, f64> ||
        std::is_same_v<T, u64> ||
        std::is_same_v<T, void*> ||
        std::is_same_v<T, ObjHandle>, "Invalid type");
    if constexpr (std::is_same_v<T, bool>) return m_Type == ValueType::Bool;
    else if constexpr (std::is_same_v<T, f64>) return m_Type == ValueType::F64;
    else if constexpr (std::is_same_v<T, u64>) return m_Type == ValueType::U64;
    else if constexpr (std::is_same_v<T, void*>) return m_Type == ValueType::Nil;
    else return m_Type == ValueType::Obj;
}

template <typename T>
const T& Value::As() const
{
    static_assert(
        std::is_same_v<T, bool> ||
        std::is_same_v<T, f64> ||
        std::is_same_v<T, u64> ||
        std::is_same_v<T, void*> ||
        std::is_same_v<T, ObjHandle>, "Invalid type");
    if constexpr (std::is_same_v<T, bool>) return m_Val.Bool;
    else if constexpr (std::is_same_v<T, f64>) return m_Val.F64;
    else if constexpr (std::is_same_v<T, u64>) return m_Val.U64;
    else if constexpr (std::is_same_v<T, void*>) return m_Val.Nil;
    else return m_Val.Obj;
}

template <typename T>
T& Value::As()
{
    return const_cast<T&>(const_cast<const Value&>(*this).As<T>());
}

template <typename T>
const T* Value::Try() const
{
    if (!HasType<T>()) return nullptr;
    return &As<T>();
}

template <typename T>
T* Value::Try() const
{
    return const_cast<T&>(const_cast<const Value&>(*this).Try<T>());
}
