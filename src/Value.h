#pragma once

#include <variant>

#include "ObjHandle.h"
#include "Types.h"

#ifndef NAN_BOXING
enum class ValueType : u8 { Bool = 1, F64 = 2, Nil = 3, Obj = 4 };
#else
using ValueType = u64;
// 0 as sign bit, then 11 + 1 + 1 bits as qNaN mark 
constexpr ValueType QNAN      = ~(1llu << (11 + 1 + 1)) << 50;
constexpr ValueType SIGN_BIT  = 1llu << 63;
constexpr ValueType TAG_NIL   = 0b01;
constexpr ValueType TAG_FALSE = 0b10;
constexpr ValueType TAG_TRUE  = 0b11;
constexpr ValueType OBJ_MASK  = SIGN_BIT | QNAN;
constexpr ValueType VAL_NIL   = QNAN | TAG_NIL;
constexpr ValueType VAL_FALSE = QNAN | TAG_FALSE;
constexpr ValueType VAL_TRUE  = QNAN | TAG_TRUE;
#endif

class Value
{
public:
    Value();
    Value(bool val);
    Value(f64 val);
    Value(void* val);
    Value(ObjHandle val);
#ifndef NAN_BOXING
    ValueType GetType() const;
#endif
    template <typename T>
    bool HasType() const;
    template <typename T>
    T As() const;
#ifdef NAN_BOXING
    auto operator<=>(const Value&) const = default;
#endif
private:
#ifdef NAN_BOXING
    ValueType m_Val{VAL_NIL};
#else
    union Val
    {
        bool Bool;
        f64 F64;
        void* Nil;
        ObjHandle Obj;
    };
    Val m_Val{.Nil = nullptr };
    ValueType m_Type{ValueType::Nil};
#endif
};

template <typename T>
bool Value::HasType() const
{
    static_assert(
        std::is_same_v<T, bool> ||
        std::is_same_v<T, f64> ||
        std::is_same_v<T, void*> ||
        std::is_same_v<T, ObjHandle>, "Invalid type");
#ifdef NAN_BOXING
    if constexpr (std::is_same_v<T, bool>) return (m_Val | 1) == VAL_TRUE;
    else if constexpr (std::is_same_v<T, f64>) return (m_Val & QNAN) != QNAN;
    else if constexpr (std::is_same_v<T, void*>) return m_Val == VAL_NIL;
    else return (m_Val & OBJ_MASK) == OBJ_MASK;
#else
    if constexpr (std::is_same_v<T, bool>) return m_Type == ValueType::Bool;
    else if constexpr (std::is_same_v<T, f64>) return m_Type == ValueType::F64;
    else if constexpr (std::is_same_v<T, void*>) return m_Type == ValueType::Nil;
    else return m_Type == ValueType::Obj;
#endif
}

template <typename T>
T Value::As() const
{
    static_assert(
        std::is_same_v<T, bool> ||
        std::is_same_v<T, f64> ||
        std::is_same_v<T, void*> ||
        std::is_same_v<T, ObjHandle>, "Invalid type");
#ifdef NAN_BOXING
    if constexpr (std::is_same_v<T, bool>) return m_Val & 1;
    else if constexpr (std::is_same_v<T, f64>) return *(f64*)&m_Val;
    else if constexpr (std::is_same_v<T, void*>) return VAL_NIL;
    else return ObjHandle{m_Val & ~(OBJ_MASK)};
#else
    if constexpr (std::is_same_v<T, bool>) return m_Val.Bool;
    else if constexpr (std::is_same_v<T, f64>) return m_Val.F64;
    else if constexpr (std::is_same_v<T, void*>) return m_Val.Nil;
    else return m_Val.Obj;
#endif
}
