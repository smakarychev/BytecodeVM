#include "Value.h"
#include "Obj.h"

Value::Value() = default;

Value::Value(bool val)
    : m_Val{.Bool = val}, m_Type(ValueType::Bool)
{
}

Value::Value(f64 val)
    : m_Val{.F64 = val}, m_Type(ValueType::F64)
{
}

Value::Value(u64 val)
    : m_Val{.U64 = val}, m_Type(ValueType::U64)
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

bool Value::HasType(ValueType type) const
{
    return m_Type == type;
}
