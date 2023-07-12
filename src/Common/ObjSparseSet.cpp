#include "ObjSparseSet.h"

#include "ValueFormatter.h"

bool ObjSparseSet::Has(ObjHandle obj) const
{
    return obj.m_ObjIndex < m_Sparse.size() && m_Sparse[obj.m_ObjIndex] != SPARSE_NONE;
}

const Value& ObjSparseSet::Get(ObjHandle obj) const
{
    return m_Dense[m_Sparse[obj.m_ObjIndex]];
}

Value& ObjSparseSet::Get(ObjHandle obj)
{
    return const_cast<Value&>(const_cast<const ObjSparseSet&>(*this).Get(obj));
}

const Value& ObjSparseSet::operator[](ObjHandle obj) const
{
    return Get(obj);
}

Value& ObjSparseSet::operator[](ObjHandle obj)
{
    return Get(obj);
}

void ObjSparseSet::Set(ObjHandle obj, Value value)
{
    if (m_Sparse.size() <= obj.m_ObjIndex)
    {
        m_Sparse.resize(obj.m_ObjIndex + 1, SPARSE_NONE);
    }
    if (m_Sparse[obj.m_ObjIndex] != SPARSE_NONE)
    {
        m_Dense[m_Sparse[obj.m_ObjIndex]] = value;
    }
    else 
    {
        m_Sparse[obj.m_ObjIndex] = m_Dense.size();
        m_Dense.push_back(value);
    }
}

ObjHandle ObjSparseSet::GetKey(u64 index)
{
    return ObjHandle{index};
}

const Value& ObjSparseSet::GetValue(u64 index)
{
    return m_Dense[m_Sparse[index]];
}
