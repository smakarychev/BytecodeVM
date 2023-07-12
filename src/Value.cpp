#include "Value.h"
#include "Obj.h"

Value::Value() = default;
#ifdef NAN_BOXING
Value::Value(bool val)
    : m_Val{VAL_FALSE | u64(val)}
{
}

Value::Value(f64 val)
    : m_Val{*(u64*)&val}
{
}

Value::Value(void* val)
    : m_Val(VAL_NIL)
{
}

Value::Value(ObjHandle val)
    : m_Val(OBJ_MASK | val.m_ObjIndex)
{
}

#else
Value::Value(bool val)
    : m_Val{.Bool = val}, m_Type(ValueType::Bool)
{
}

Value::Value(f64 val)
    : m_Val{.F64 = val}, m_Type(ValueType::F64)
{
}

Value::Value(void* val)
{
}

Value::Value(ObjHandle val)
    : m_Val{.Obj = val}, m_Type(ValueType::Obj)
{
}

ValueType Value::GetType() const
{
    return m_Type;
}
#endif


